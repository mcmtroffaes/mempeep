#include <cassert>      // assert
#include <cstdint>      // std::int32_t, ...
#include <cstring>      // std::memcpy
#include <format>       // std::format
#include <functional>   // std::function
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
concept IsMember = std::is_member_object_pointer_v<decltype(M)>;

template <auto M>
  requires IsMember<M>
struct member_traits;

template <typename C, typename T, T C::* M>
struct member_traits<M> {
  using type = T;
};

template <auto M>
  requires IsMember<M>
using member_type_t = typename member_traits<M>::type;

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

struct EmptyScope {};

// Deduces Tracer::Scope from Tracer, avoiding repetition at call sites
template <IsTracer Tracer, IsAddress Address, typename Label>
  requires std::convertible_to<Label, std::string_view>
auto make_scope(Tracer& tracer, Address address, Label&& label) {
  if constexpr (IsScopedTracer<Tracer>) {
    const auto addr = static_cast<std::uint64_t>(address);
    return typename Tracer::Scope(tracer, addr, label);
  } else {
    return EmptyScope{};
  }
}

// Simple scoped tracer which prints errors along with the address where they
// occurred.
struct PrintTracer : ErrorTracer {
  int indent = 0;
  uint64_t address = 0;

  void log(std::string_view msg) {
    auto whitespace = std::string(indent, ' ');
    std::cout << std::format("[{:08X}] ", address) << whitespace << whitespace
              << msg << std::endl;
  }

  void error(std::string_view reason) {
    ErrorTracer::error(reason);  // set flag
    log(std::format("ERROR: {}", reason));
  }

  struct Scope {
    PrintTracer& t;

    Scope(PrintTracer& _t, uint64_t address, std::string_view label) : t(_t) {
      t.address = address;
      t.log(label);
      t.indent++;
    }

    ~Scope() { t.indent--; }
  };
};

static_assert(IsTracer<ErrorTracer>);
static_assert(!IsScopedTracer<ErrorTracer>);
static_assert(IsScopedTracer<PrintTracer>);

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
  = std::is_standard_layout_v<T> && std::is_default_constructible_v<T>;

/**
 * @brief Tag for registering the remote layout of a native struct.
 *
 * Example:
 *
 *   auto layout_of(LayoutOf<Pos>) -> Layout<Field<&Pos::x>, Pad<4>,
 * Field<&Pos::y>>;
 *
 * @tparam T Native struct to register the layout for.
 */
template <typename T>
  requires IsReadable<T>
struct LayoutOf {};

/**
 * @brief Does T have a custom layout?
 *
 * Checks if the function layout_of(LayoutOf<T>) exists.
 */
template <typename T>
concept HasLayout = requires { layout_of(LayoutOf<T>{}); };

/**
 * @brief Shorthand for the return type of `layout_of(LayoutOf<T>{})`.
 */
template <typename T>
  requires HasLayout<T>
using layout_of_t = decltype(layout_of(LayoutOf<T>{}));

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
 * @tparam N Number of bytes (strictly positive).
 *           Its value must be representable by address_t<MemoryReader>.
 *           The read template will not instantiate otherwise.
 */
template <auto N>
  requires(std::integral<decltype(N)> && N > 0)
struct Pad {
  using layout_item_tag = void;
  static constexpr std::size_t count = static_cast<std::size_t>(N);
};

/**
 * @brief Absolute offset relative to base position of the layout.
 * @tparam N The offset in bytes (strictly positive).
 *           Its value must be representable by address_t<MemoryReader>.
 *           The read template will not instantiate otherwise.
 */
template <auto N>
  requires(std::integral<decltype(N)> && N > 0)
struct Seek {
  using layout_item_tag = void;
  static constexpr std::size_t offset = static_cast<std::size_t>(N);
};

/**
 * @brief Raw address, not followed.
 *
 * Important: Always reads exactly sizeof(address_type) bytes, not
 * sizeof(member_type_t<M>). Result is cast to wider type if needed.
 * If the member type is too narrow, a compile error results.
 *
 * @tparam M The native field to deserialize the address into.
 *           Its type must be wide enough to hold address_t<MemoryReader>.
 *           The read template will not instantiate otherwise.
 */
template <auto M>
  requires IsAddress<member_type_t<M>>
struct Ptr {
  using layout_item_tag = void;
};

/**
 * @brief Follow address and read pointee.
 * @tparam M The native field to deserialize the pointee into.
 */
template <auto M>
  requires IsReadable<member_type_t<M>>
struct Ref {
  using layout_item_tag = void;
};

/**
 * @brief Follow address and read pointee if not null.
 * @tparam M The native field to deserialize the pointee into.
 *           Must have type std::optional<T>.
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
 * @param address Remote source pointer.
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

// Stores result of reading: address after reading, or null if read failed.
// Note error messages are propagated separately through the tracer.
template <IsMemoryReader MemoryReader>
using NextAddress = std::optional<address_t<MemoryReader>>;

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

// Forward declaration to support recursive reading.
template <IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
[[nodiscard]] NextAddress<MemoryReader> read(
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
[[nodiscard]] NextAddress<MemoryReader> read_layout_item(
  Pad<N> item,
  const MemoryReader&,
  address_t<MemoryReader>,
  address_t<MemoryReader> address,
  auto&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope
    = make_scope(tracer, address, std::format("pad(0x{:X})", item.count));
  return traced_advance(address, item.count, tracer);
}

template <auto N, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] NextAddress<MemoryReader> read_layout_item(
  Seek<N> item,
  const MemoryReader&,
  address_t<MemoryReader> base,
  address_t<MemoryReader> address,
  auto&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope
    = make_scope(tracer, address, std::format("offset(0x{:X})", item.offset));
  return traced_advance(base, item.offset, tracer);
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<member_type_t<M>>
[[nodiscard]] NextAddress<MemoryReader> read_layout_item(
  Field<M> item,
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
[[nodiscard]] NextAddress<MemoryReader> read_address_into(
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
[[nodiscard]] NextAddress<MemoryReader> read_layout_item(
  Ptr<M>,
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  return read_address_into(reader, address, target.*M, tracer);
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<member_type_t<M>>
[[nodiscard]] NextAddress<MemoryReader> read_layout_item(
  Ref<M>,
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  address_t<MemoryReader> target_ptr{};
  auto next_addr = read_address_into(reader, address, target_ptr, tracer);
  if (!next_addr) return {};
  if (target_ptr) {
    // ignore output: errors reported already, no need for extra error message
    std::ignore = read(reader, target_ptr, target.*M, tracer);
  } else {
    tracer.error("null pointer");
  }
  return next_addr;
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<unwrap_optional_t<member_type_t<M>>>
[[nodiscard]] NextAddress<MemoryReader> read_layout_item(
  NullableRef<M> item,
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  address_t<MemoryReader> target_ptr{};
  auto next_addr = read_address_into(reader, address, target_ptr, tracer);
  if (!next_addr) return {};
  auto& field = target.*M;
  field.reset();
  if (target_ptr) {
    auto& target_value = field.emplace();
    // ignore output: errors reported already, no need for extra error message
    std::ignore = read(reader, target_ptr, target_value, tracer);
    // note: keep field emplaced even if read fails
  }
  // note: null target_ptr is ok, no error reported
  return next_addr;
}

template <
  IsLayoutItem... Items,
  IsMemoryReader MemoryReader,
  IsReadable T,
  IsTracer Tracer>
[[nodiscard]] NextAddress<MemoryReader> read_layout(
  Layout<Items...>,
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  T& target,
  Tracer& tracer
) {
  NextAddress<MemoryReader> next_addr{base};
  // - ((expr), ...) is intentional: comma has the lowest precedence
  // - comma operator sequences the effects from left to right
  // - && short-circuits the evaluation so we stop on first failure
  // - Items{} is just a tag to select the overload, construction costs nothing
  // We can read this comma fold as follows:
  // If the current next_addr (i.e. address position) is valid (i.e. evaluates
  // to true) then update it using read_layout_item, and now keep repeating that
  // for all items.
  ((
     next_addr
     && (next_addr = read_layout_item(Items{}, reader, base, next_addr.value(), target, tracer))
   ),
   ...);
  return next_addr;
}

template <IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
[[nodiscard]] NextAddress<MemoryReader> read(
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  T& target,
  Tracer& tracer
) {
  if constexpr (HasLayout<T>) {
    return detail::read_layout(layout_of_t<T>{}, reader, base, target, tracer);
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

// example with 16 bit pointers, for fun
template <auto BASE, auto N>
  requires(
    std::is_integral_v<decltype(BASE)> && std::is_integral_v<decltype(N)>
    && BASE > 0 && BASE <= UINT16_MAX && N > 0 && N <= SIZE_MAX
  )
struct MockMemoryReader {
  static const auto BASE_ = static_cast<uint16_t>(BASE);
  static const auto N_ = static_cast<std::size_t>(N);
  std::byte data[N]{};
  using address_type = uint16_t;

  bool operator()(uint16_t address, std::size_t size, void* buffer) const {
    // check buffer exists and size is positive
    if (!buffer) return false;
    if (!size) return false;
    // handle overflow
    if (N_ < size) return false;        // ensure N_ - size >= 0
    if (address < BASE_) return false;  // ensure address - BASE_ >= 0
    if (N_ - size < address - BASE_) return false;  // ensure no overread
    std::memcpy(buffer, data + (address - BASE_), size);
    return true;
  }

  template <auto OFFSET, typename T>
    requires(
      std::is_integral_v<decltype(OFFSET)> && OFFSET >= 0
      && OFFSET <= UINT16_MAX
    )
  void write(T value) {
    constexpr auto OFFSET_ = static_cast<uint16_t>(OFFSET);
    static_assert(N_ >= sizeof(T));   // ensure N_ - size >= 0
    static_assert(OFFSET_ >= BASE_);  // ensure OFFSET_ - BASE_ >= 0
    static_assert(N_ - sizeof(T) >= OFFSET_ - BASE_);  // ensure no overwrite
    std::memcpy(data + (OFFSET_ - BASE_), &value, sizeof(value));
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
auto layout_of(LayoutOf<Pos>)
  -> Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>, Pad<4>>;

auto layout_of(LayoutOf<Player>) -> Layout<
  Seek<8>,
  Field<&Player::health>,
  Seek<16>,
  Field<&Player::pos>,
  Ptr<&Player::target_ptr>,
  Ptr<&Player::shop_ptr>,
  Ptr<&Player::weapon_ptr>,
  Ref<&Player::prev_pos>,
  NullableRef<&Player::tagged_pos>,
  NullableRef<&Player::house_pos>,
  Field<&Player::mana>>;

auto layout_of(LayoutOf<Game>) -> Layout<Seek<6>, Field<&Game::player>>;

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
  MockMemoryReader<1, 128> reader{};
  reader.write<18>(int32_t{123});  // health
  reader.write<26>(int32_t{11});   // pos.x
  reader.write<34>(int32_t{22});   // pos.y
  reader.write<42>(uint16_t{0});   // target_ptr
  reader.write<44>(uint16_t{2});   // shop_ptr; u16 remote, u32 native
  reader.write<46>(uint16_t{6});   // weapon_ptr
  reader.write<48>(uint16_t{60});  // prev_pos ref
  reader.write<50>(uint16_t{80});  // tagged_pos nullable ref
  reader.write<52>(uint16_t{0});   // house_pos nullable ref
  reader.write<54>(int32_t{47});   // mana
  reader.write<60>(int32_t{88});   // prev_pos.x
  reader.write<68>(int32_t{99});   // prev_pos.y
  reader.write<80>(int32_t{55});   // tagged_pos.x
  reader.write<88>(int32_t{66});   // tagged_pos.y
  uint16_t base{4};

  {
    Game game{};
    assert(mempeep::read_remote(reader, base, game));
  }
  {
    Game game{};
    assert(mempeep::read_remote(reader, base, game, PrintTracer{}));
  }
}
