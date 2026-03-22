#include <cassert>      // assert
#include <cstdint>      // std::int32_t, ...
#include <cstring>      // std::memcpy
#include <format>       // std::format
#include <iostream>     // std::cout, ...
#include <optional>     // std::optional
#include <type_traits>  // std::same_as, ...
#include <utility>      // std::cmp_less_equal

namespace mempeep {

// ============================================================
// Integer concepts
// ============================================================

// possible user address types
template <typename T>
concept IsAddress = std::unsigned_integral<T> && !std::same_as<T, bool>
                    && !std::same_as<T, char>;

// ============================================================
// Member traits
// ============================================================

template <auto M>
  requires std::is_member_object_pointer_v<decltype(M)>
struct member_type;

template <typename C, typename T, T C::* M>
struct member_type<M> {
  using type = T;
};

template <auto M>
using member_type_t = typename member_type<M>::type;

template <typename T>
struct unwrap_optional {};

template <typename U>
struct unwrap_optional<std::optional<U>> {
  using type = U;
};

template <typename T>
using unwrap_optional_t = typename unwrap_optional<T>::type;

// ============================================================
// Tracing
// ============================================================

// note: uint64_t address avoids dependence on MemoryReader
// this is ok for logging, and keeps implementation simple
template <typename Tracer>
concept IsTracer = requires(Tracer& tracer, std::string_view s) {
  { tracer.error(s) } -> std::same_as<void>;          // report error
  { tracer.success() } -> std::convertible_to<bool>;  // any error reported?
};

template <typename Tracer>
concept IsScopedTracer
  = IsTracer<Tracer>
    && requires(Tracer& tracer, std::uint64_t addr, std::string_view s) {
         typename Tracer::Scope;
         { typename Tracer::Scope(tracer, addr, s) };
       };

// Minimal tracer that detects if any error has occurred
struct ErrorTracer {
  bool ok = true;

  void error(std::string_view) { ok = false; }

  bool success() const { return ok; }
};

struct NoScope {};

// Deduces Tracer::Scope from Tracer, avoiding repetition at call sites
template <IsTracer Tracer, IsAddress Address, typename Label>
  requires std::convertible_to<Label, std::string_view>
auto make_scope(Tracer& tracer, Address address, Label&& label) {
  if constexpr (IsScopedTracer<Tracer>) {
    const auto addr = static_cast<std::uint64_t>(address);
    return typename Tracer::Scope(tracer, addr, label);
  } else {
    return NoScope{};
  }
}

// Simple scoped tracer which prints errors along with the address where they
// occurred.
struct LogTracer : ErrorTracer {
  std::ostream& out;
  int indent = 0;
  uint64_t address = 0;

  explicit LogTracer(std::ostream& out = std::cout) : out(out) {}

  void log(std::string_view msg) {
    const auto whitespace = std::string(indent, ' ');
    out << std::format("[{:08X}] ", address) << whitespace << msg << std::endl;
  }

  void error(std::string_view reason) {
    ErrorTracer::error(reason);  // set flag
    log(std::format("ERROR: {}", reason));
  }

  struct Scope {
    LogTracer& t;

    Scope(LogTracer& _t, uint64_t address, std::string_view label) : t(_t) {
      t.address = address;
      t.log(label);
      t.indent++;
    }

    ~Scope() { t.indent--; }
  };
};

static_assert(IsTracer<ErrorTracer>);
static_assert(!IsScopedTracer<ErrorTracer>);
static_assert(IsScopedTracer<LogTracer>);

template <auto M>
consteval std::string_view member_name() {
#if defined(_MSC_VER)
  constexpr std::string_view sig = __FUNCSIG__;
  constexpr auto last_colon = sig.rfind(':');
  constexpr auto close = sig.rfind('>');
#else
  constexpr std::string_view sig = __PRETTY_FUNCTION__;
  constexpr auto last_colon = sig.rfind(':');
  constexpr auto close = sig.rfind(']');
#endif
  static_assert(
    last_colon != std::string_view::npos,
    "member_name(): failed to find ':' in function signature"
  );
  static_assert(
    close != std::string_view::npos,
    "member_name(): failed to find closing delimiter in function signature"
  );
  static_assert(
    last_colon + 1 < close, "member_name(): invalid substring bounds"
  );
  constexpr auto len = close - last_colon - 1;
  static_assert(len > 0, "member_name(): extracted name is empty");
  return sig.substr(last_colon + 1, close - last_colon - 1);
}

// ============================================================
// Layout items
// ============================================================

template <typename T>
concept IsLayoutItem = requires { typename T::layout_item_tag; };

/**
 * @brief Defines a remote layout.
 *
 * Each item must be a Field, Pad, or Seek.
 * Example:
 *
 *   Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>>
 *
 * @tparam Items Sequence of Field, Seek, and Pad types.
 */
template <IsLayoutItem... Items>
struct Layout {};

template <typename T>
concept IsReadable
  = std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T>;
;

/**
 * @brief Tag for registering the remote layout of a native struct.
 *
 * Example:
 *
 *   auto remote_layout(remote_layout_tag<Pos>)
 *     -> Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>>;
 *
 * @tparam T Native struct to register the layout for.
 */
template <typename T>
  requires IsReadable<T>
struct remote_layout_tag {};

/**
 * @brief Does T have a custom layout?
 *
 * Checks if the function remote_layout(remote_layout_tag<T>) exists.
 */
template <typename T>
concept HasRemoteLayout = requires { remote_layout(remote_layout_tag<T>{}); };

/**
 * @brief Shorthand for return type of `remote_layout(remote_layout_tag<T>{})`.
 */
template <typename T>
  requires HasRemoteLayout<T>
using remote_layout_t = decltype(remote_layout(remote_layout_tag<T>{}));

/**
 * @brief A field. Its type must be readable.
 *
 * For example, Field<&Class::member>.
 *
 * @tparam M The native field to deserialize into.
 */
template <auto M>
  requires IsReadable<member_type_t<M>>
struct Field {
  using layout_item_tag = void;
};

/**
 * @brief Padding relative to the current position in the layout.
 * @tparam N Number of bytes (strictly positive; zero is excluded as
 *           zero padding is never needed in practice).
 *           Its value must be representable by address_t<MemoryReader>.
 */
template <auto N>
  requires(std::in_range<std::size_t>(N) && N > 0)
struct Pad {
  using layout_item_tag = void;
  static constexpr std::size_t count = static_cast<std::size_t>(N);
};

/**
 * @brief Absolute offset relative to base position of the layout.
 *
 * Seeks are not required to be monotonically increasing. This allows
 * skipping around a non-linear layout. It is the caller's responsibility to
 * ensure the offsets are correct.
 *
 * @tparam N The offset in bytes (strictly positive; zero is excluded as
 *           jumping to base is never needed in practice).
 *           Its value must be representable by address_t<MemoryReader>.
 */
template <auto N>
  requires(std::in_range<std::size_t>(N) && N > 0)
struct Seek {
  using layout_item_tag = void;
  static constexpr std::size_t offset = static_cast<std::size_t>(N);
};

/**
 * @brief Read a remote address without following it, storing it in the member.
 *
 * Always reads exactly sizeof(address_type) bytes, not
 * sizeof(member_type_t<M>). The result is cast to the member type, which must
 * be wide enough to hold address_t<MemoryReader>, or a compile error results.
 *
 * @tparam M The native field to store the address into.
 */
template <auto M>
  requires IsAddress<member_type_t<M>>
struct RawAddr {
  using layout_item_tag = void;
};

/**
 * @brief Read a remote address and follow it, reading the pointee into the
 * member. The address itself is not stored.
 *
 * Errors if the address is null.
 *
 * @tparam M The native field to deserialize the pointee into.
 */
template <auto M>
  requires IsReadable<member_type_t<M>>
struct Ref {
  using layout_item_tag = void;
};

/**
 * @brief Read a remote address and follow it, reading the pointee into the
 * member. The address itself is not stored.
 *
 * If the address is null, the member is set to std::nullopt and no error
 * is reported.
 *
 * @tparam M The native field to deserialize the pointee into.
 *           Must have type std::optional<T> where T is readable.
 */
template <auto M>
  requires IsReadable<unwrap_optional_t<member_type_t<M>>>
struct NullableRef {
  using layout_item_tag = void;
};

/**
 * @brief Extract address_type from MemoryReader.
 */
template <typename MemoryReader>
using address_t = typename MemoryReader::address_type;

/**
 * @brief Functor concept to read a block of memory from a remote source.
 *
 * Copy `size` bytes into `buffer` from remote memory at `address`.
 * On failure, return false, otherwise return true.
 *
 * @param address Remote source address.
 * @param size    Number of bytes to read.
 * @param buffer  Native destination buffer.
 * @return        `true` on success, `false` on failure.
 */
template <typename MemoryReader>
concept IsMemoryReader = requires(
  MemoryReader reader,
  address_t<MemoryReader> address,
  std::size_t size,
  void* buffer
) {
  { reader(address, size, buffer) } -> std::same_as<bool>;
};

template <typename T, typename MemoryReader>
concept CanStoreAddressOf
  = IsAddress<T>
    && std::numeric_limits<address_t<MemoryReader>>::max()
         <= std::numeric_limits<T>::max();

// Abstract unsigned addition with overflow check.
template <std::unsigned_integral S, std::unsigned_integral T>
[[nodiscard]] constexpr std::optional<S> checked_add(S s, T t) noexcept {
  if (t > std::numeric_limits<S>::max() - s) return {};
  return static_cast<S>(s + t);
}

// Advance address by n with traced error in case of overflow.
template <IsAddress Addr, IsTracer Tracer>
[[nodiscard]] std::optional<Addr> traced_advance(
  Addr addr, std::size_t n, Tracer& tracer
) {
  auto u = checked_add(addr, n);
  if (!u) tracer.error("remote address overflow");
  return u;
}

namespace detail {

// ============================================================
// Helpers
// ============================================================

// Cursor tracks the current read position within a layout.
//
// It starts at the layout's base address and advances as each item is read.
// It becomes nullopt when the current position can no longer be determined,
// for example, due to a failed memory read or an address overflow.
//
// Key invariant: a cursor only becomes nullopt if an error was already
// reported to the tracer. nullopt without a tracer error never occurs.
//
// Errors are contained locally: a child layout's cursor becoming nullopt
// does not invalidate the parent's cursor. This allows reading to continue
// past failed items (e.g. a bad address), recovering as much data as
// possible. Child errors are still reported through the tracer.
template <IsMemoryReader MemoryReader>
using Cursor = std::optional<address_t<MemoryReader>>;

// Forward declaration to support recursive reading.
template <IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read(
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  T& target,
  Tracer& tracer
);

// thin wrapper around reader with tracing
template <IsMemoryReader MemoryReader, IsTracer Tracer, typename T>
[[nodiscard]] bool read_bytes(
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  if (!reader(address, sizeof(target), &target)) {
    tracer.error("memory read failed");
    return false;
  }
  return true;
}

template <auto N, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  Pad<N>,
  const MemoryReader&,
  address_t<MemoryReader>,
  address_t<MemoryReader> address,
  auto&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope
    = make_scope(tracer, address, std::format("pad(0x{:X})", Pad<N>::count));
  return traced_advance(address, Pad<N>::count, tracer);
}

template <auto N, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  Seek<N>,
  const MemoryReader&,
  address_t<MemoryReader> base,
  address_t<MemoryReader> address,
  auto&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(
    tracer, address, std::format("offset(0x{:X})", Seek<N>::offset)
  );
  return traced_advance(base, Seek<N>::offset, tracer);
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<member_type_t<M>>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  Field<M>,
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  auto& field = target.*M;
  return read(reader, address, field, tracer);
}

template <typename T, IsMemoryReader MemoryReader, IsTracer Tracer>
  requires CanStoreAddressOf<T, MemoryReader>
[[nodiscard]] Cursor<MemoryReader> read_address_into(
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  address_t<MemoryReader> ptr{};
  if (!read_bytes(reader, address, ptr, tracer)) return {};
  // static_cast safe by CanStoreAddressOf
  target = static_cast<T>(ptr);
  return traced_advance(address, sizeof(ptr), tracer);
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  RawAddr<M>,
  const MemoryReader& reader,
  address_t<MemoryReader>,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  return read_address_into(reader, address, target.*M, tracer);
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<member_type_t<M>>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  Ref<M>,
  const MemoryReader& reader,
  address_t<MemoryReader>,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  address_t<MemoryReader> target_ptr{};
  auto cursor = read_address_into(reader, address, target_ptr, tracer);
  if (!cursor) return {};
  if (target_ptr) {
    // ignore output: errors reported already, no need for extra error message
    std::ignore = read(reader, target_ptr, target.*M, tracer);
  } else {
    tracer.error("null address");
  }
  return cursor;
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<unwrap_optional_t<member_type_t<M>>>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  NullableRef<M>,
  const MemoryReader& reader,
  address_t<MemoryReader>,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  address_t<MemoryReader> target_ptr{};
  auto cursor = read_address_into(reader, address, target_ptr, tracer);
  if (!cursor) return {};
  auto& field = target.*M;
  field.reset();
  if (target_ptr) {
    auto& target_value = field.emplace();
    // ignore output: errors reported already, no need for extra error message
    std::ignore = read(reader, target_ptr, target_value, tracer);
    // note: keep field emplaced even if read fails
  }
  // note: null target_ptr is ok, no error reported
  return cursor;
}

template <
  IsLayoutItem... Items,
  IsMemoryReader MemoryReader,
  IsReadable T,
  IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_layout(
  Layout<Items...>,
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  T& target,
  Tracer& tracer
) {
  Cursor<MemoryReader> cursor{base};
  // Process each layout item in order, stopping if the cursor becomes nullopt.
  // This is a comma fold: (expr, ...) evaluates each expr left-to-right.
  // Each expr is: cursor && (cursor = read_layout_item(...))
  // The && is plain short-circuit evaluation, not a fold operator:
  // if cursor is nullopt (falsy), the assignment is skipped.
  // Items{} constructs a tag value at zero cost to select the right overload.
  ((
     cursor
     && (cursor = read_layout_item(Items{}, reader, base, cursor.value(), target, tracer))
   ),
   ...);
  return cursor;
}

template <IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read(
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  T& target,
  Tracer& tracer
) {
  if constexpr (HasRemoteLayout<T>) {
    return detail::read_layout(
      remote_layout_t<T>{}, reader, base, target, tracer
    );
  } else {
    if (!detail::read_bytes(reader, base, target, tracer)) return {};
    return traced_advance(base, sizeof(target), tracer);
  }
}

}  // namespace detail

// ============================================================
// Public API: read
// ============================================================

/**
 * @brief Reads data from remote memory into a native object based on a
 * specified layout.
 *
 * The function will try to read as much data as possible, i.e. even after
 * failing to read subfields.
 * Returns the result of `tracer.success()`.
 * By convention, this value is convertible to bool, and evaluates to true
 * if no errors were reported, and false otherwise.
 * The default tracer does exactly this.
 *
 * @tparam MemoryReader The type for the reader callback.
 * @tparam T          The native type to deserialize into.
 * @param memory The memory abstraction providing the `MemoryReader` function.
 * @param base   The remote address to start reading from.
 * @param target The native object to populate.
 * @return The result of `tracer.success()` (convertible to bool).
 */
template <
  IsMemoryReader MemoryReader,
  IsReadable T,
  IsTracer Tracer = ErrorTracer>
auto read_remote(
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  T& target,
  Tracer tracer = {}  // by value: caller's tracer is not modified
) {
  // Passing tracer by reference internally so all recursive calls share
  // the same state, without copying on each call.
  std::ignore = detail::read(reader, base, target, tracer);
  return tracer.success();
}

}  // namespace mempeep

// ============================================================
// Example
// ============================================================

// mock reader for testing
template <typename Address, auto BASE, auto N>
  requires(
    IsAddress<Address> && std::in_range<Address>(BASE)
    && std::in_range<std::size_t>(N)
  )
struct MockMemoryReader {
  using address_type = Address;
  static constexpr Address base = static_cast<Address>(BASE);
  static constexpr std::size_t n = static_cast<std::size_t>(N);
  std::byte data[n]{};

  bool operator()(Address address, std::size_t size, void* buffer) const {
    // check buffer exists and n is positive
    if (buffer == nullptr) return false;
    if (size == 0) return false;
    // handle overflow
    if (n < size) return false;        // ensure n - size >= 0
    if (address < base) return false;  // ensure address - base >= 0
    // both sides of inequality below are non-negative so safe to compare
    if (n - size < address - base) return false;  // ensure no overread
    // address - base <= n so now it is safe to cast to std::size_t
    std::memcpy(buffer, data + static_cast<std::size_t>(address - base), size);
    return true;
  }

  // note: this function is only safe in debug mode
  template <typename Addr, typename T>
  void write(Addr addr, T value) {
    assert(std::in_range<Address>(addr));
    const auto address = static_cast<Address>(addr);
    assert(n >= sizeof(T));   // ensure n - sizeof(T) >= 0
    assert(address >= base);  // ensure off - base >= 0
    // both sides of inequality below are non-negative so safe to compare
    assert(n - sizeof(T) >= address - base);  // ensure no overwrite
    // address - base <= n so now it is safe to cast to std::size_t
    std::memcpy(
      data + static_cast<std::size_t>(address - base), &value, sizeof(value)
    );
  }
};

struct Pos {
  int32_t x, y;
};

struct Player {
  int32_t health;
  Pos pos;
  uint16_t target_ptr;
  uint32_t shop_ptr;  // wider than needed, still fine
  uint16_t weapon_ptr;
  Pos prev_pos;
  std::optional<Pos> tagged_pos;
  std::optional<Pos> house_pos;
  int32_t mana;
};

struct Game {
  Player player;
};

using namespace mempeep;

static_assert(member_name<&Player::house_pos>() == "house_pos");

// intentionally have 4 padding bytes at end, for testing
auto remote_layout(remote_layout_tag<Pos>)
  -> Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>, Pad<4>>;

auto remote_layout(remote_layout_tag<Player>) -> Layout<
  Seek<8>,
  Field<&Player::health>,
  Seek<16>,
  Field<&Player::pos>,
  RawAddr<&Player::target_ptr>,
  RawAddr<&Player::shop_ptr>,
  RawAddr<&Player::weapon_ptr>,
  Ref<&Player::prev_pos>,
  NullableRef<&Player::tagged_pos>,
  NullableRef<&Player::house_pos>,
  Field<&Player::mana>>;

auto remote_layout(remote_layout_tag<Game>)
  -> Layout<Seek<6>, Field<&Game::player>>;

static void assert_game(const Game& game) {
  assert(game.player.health == 123);
  assert(game.player.pos.x == 11);
  assert(game.player.pos.y == 22);
  assert(game.player.target_ptr == 0);
  assert(game.player.shop_ptr == 2);
  assert(game.player.weapon_ptr == 6);
  assert(game.player.prev_pos.x == 88);
  assert(game.player.prev_pos.y == 99);
  assert(game.player.mana == 47);
  assert(game.player.tagged_pos.has_value());
  assert(game.player.tagged_pos->x == 55);
  assert(game.player.tagged_pos->y == 66);
  assert(!game.player.house_pos.has_value());
}

int main() {
  MockMemoryReader<uint16_t, 1, 128> reader{};
  reader.write(18, int32_t{123});  // health
  reader.write(26, int32_t{11});   // pos.x
  reader.write(34, int32_t{22});   // pos.y
  reader.write(42, uint16_t{0});   // target_ptr
  reader.write(44, uint16_t{2});   // shop_ptr; u16 remote, u32 native
  reader.write(46, uint16_t{6});   // weapon_ptr
  reader.write(48, uint16_t{60});  // prev_pos ref
  reader.write(50, uint16_t{80});  // tagged_pos nullable ref
  reader.write(52, uint16_t{0});   // house_pos nullable ref
  reader.write(54, int32_t{47});   // mana
  reader.write(60, int32_t{88});   // prev_pos.x
  reader.write(68, int32_t{99});   // prev_pos.y
  reader.write(80, int32_t{55});   // tagged_pos.x
  reader.write(88, int32_t{66});   // tagged_pos.y
  uint16_t base{4};

  {
    Game game{};
    assert(mempeep::read_remote(reader, base, game));
  }
  {
    Game game{};
    assert(mempeep::read_remote(reader, base, game, LogTracer{}));
  }
}
