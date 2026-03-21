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
  using member_type = T;
};

template <auto M>
  requires IsMember<M>
using member_type_t = typename member_traits<M>::member_type;

// helper to detect std::optional
template <typename T>
struct is_optional : std::false_type {};

template <typename U>
struct is_optional<std::optional<U>> : std::true_type {};

template <auto M>
  requires is_optional<member_type_t<M>>::value
struct member_optional_traits;

template <typename C, typename U, std::optional<U> C::* M>
struct member_optional_traits<M> {
  using optional_value_type = U;
};

template <auto M>
  requires is_optional<member_type_t<M>>::value
using optional_value_type_t =
  typename member_optional_traits<M>::optional_value_type;

// ============================================================
// Tracing
// ============================================================

// note: uint64_t address avoids dependence on MemoryReader
// this is ok for logging, and keeps implementation simple
template <typename Tracer>
concept IsTracer
  = requires(Tracer& tracer, std::uint64_t address, std::string_view s) {
      typename Tracer::Scope;
      { Tracer::Scope(tracer, address, s) };
      { tracer.error(s) } -> std::same_as<void>;
    };

// Disables tracing
struct NoTracer {
  struct Scope {
    Scope() = default;

    Scope(NoTracer&, std::uint64_t, std::string_view) {}
  };

  void error(std::string_view) {}
};

// Deduces Tracer::Scope from Tracer, avoiding repetition at call sites
template <IsTracer Tracer, IsAddress Address>
auto make_scope(Tracer& tracer, Address address, std::string_view label) {
  if constexpr (std::same_as<Tracer, NoTracer>) {
    return NoTracer::Scope{};  // empty, zero cost
  } else {
    return typename Tracer::Scope(
      tracer, static_cast<std::uint64_t>(address), label
    );
  }
}

// Returns the scope so the caller controls its lifetime
template <IsTracer Tracer, IsAddress Address, typename F>
  requires std::invocable<F>
           && std::same_as<std::invoke_result_t<F>, std::string>
auto make_lazy_scope(Tracer& tracer, Address address, F&& label_fn) {
  if constexpr (std::same_as<Tracer, NoTracer>) {
    return NoTracer::Scope{};
  } else {
    return typename Tracer::Scope(
      tracer, static_cast<std::uint64_t>(address), label_fn()
    );
  }
}

// Simple tracer which prints errors along with the address where they occurred.
struct PrintTracer {
  int indent = 0;
  uint64_t address = 0;

  void error(std::string_view reason) {
    auto whitespace = std::string(indent, ' ');
    std::cout << std::format("[{:08X}] ", address) << whitespace << whitespace
              << reason << std::endl;
  }

  struct Scope {
    PrintTracer& t;

    Scope(PrintTracer& _t, uint64_t address, std::string_view label) : t(_t) {
      t.address = address;
      t.error(label);
      t.indent++;
    }

    ~Scope() { t.indent--; }
  };
};

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

/**
 * @brief For tagging layout items.
 *
 * The IsLayoutItem concept selects those types that have this tag.
 */
struct LayoutItem {};

template <typename T>
concept IsLayoutItem
  = std::is_base_of_v<LayoutItem, T> && !std::same_as<T, LayoutItem>;

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
struct Field : LayoutItem {};

/**
 * @brief Padding relative to the current position in the layout.
 * @tparam N Number of bytes (strictly positive).
 *           Its value must be representable by pointer_type_t<MemoryReader>.
 *           The read template will not instantiate otherwise.
 */
template <auto N>
  requires(std::integral<decltype(N)> && N > 0)
struct Pad : LayoutItem {
  static constexpr std::size_t count = static_cast<std::size_t>(N);
};

/**
 * @brief Absolute offset relative to base position of the layout.
 * @tparam N The offset in bytes (non-negative; 0 allowed to seek to start).
 *           Its value must be representable by pointer_type_t<MemoryReader>.
 *           The read template will not instantiate otherwise.
 */
template <auto N>
  requires(std::integral<decltype(N)> && N >= 0)
struct Seek : LayoutItem {
  static constexpr std::size_t offset = static_cast<std::size_t>(N);
};

/**
 * @brief Raw address, not followed.
 * @tparam M The native field to deserialize the address into.
 *           Its type must be wide enough to hold pointer_type_t<MemoryReader>.
 *           The read template will not instantiate otherwise.
 */
template <auto M>
  requires IsAddress<member_type_t<M>>
struct Ptr : LayoutItem {};

/**
 * @brief Follow address and read pointee.
 * @tparam M The native field to deserialize the pointee into.
 */
template <auto M>
  requires IsReadable<member_type_t<M>>
struct Ref : LayoutItem {};

/**
 * @brief Follow address and read pointee if not null.
 * @tparam M The native field to deserialize the pointee into.
 *           Must have type std::optional<T>.
 */
template <auto M>
  requires IsReadable<optional_value_type_t<M>>
struct NullableRef : LayoutItem {};

/**
 * @brief Extract pointer_type from MemoryReader.
 */
template <typename MemoryReader>
using pointer_type_t = typename MemoryReader::pointer_type;

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
  pointer_type_t<MemoryReader> address,
  std::size_t size,
  void* buffer
) {
  { reader(address, size, buffer) } -> std::same_as<bool>;
};

template <typename T, typename MemoryReader>
concept CanStoreAddressOf
  = IsAddress<T>
    && std::numeric_limits<pointer_type_t<MemoryReader>>::max()
         <= std::numeric_limits<T>::max();

// Stores result of reading: address after reading, or null if read failed.
// Note error messages are propagated separately through the tracer.
template <IsMemoryReader MemoryReader>
using ReadCursor = std::optional<pointer_type_t<MemoryReader>>;

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

// Forward declaration to support recursive reading.
template <IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryReader> read_remote_into(
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  T& target,
  Tracer& tracer
);

namespace detail {

// ============================================================
// Helpers
// ============================================================

// thin wrapper around reader with tracing
template <IsMemoryReader MemoryReader, IsTracer Tracer, typename T>
[[nodiscard]] bool read_bytes(
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> address,
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
[[nodiscard]] ReadCursor<MemoryReader> read_layout_item(
  Pad<N> item,
  const MemoryReader&,
  pointer_type_t<MemoryReader> base,
  pointer_type_t<MemoryReader> address,
  auto&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_lazy_scope(tracer, address, [&] {
    return std::format("pad(0x{:X})", item.count);
  });
  return traced_advance(address, item.count, tracer);
}

template <auto N, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryReader> read_layout_item(
  Seek<N> item,
  const MemoryReader&,
  pointer_type_t<MemoryReader> base,
  pointer_type_t<MemoryReader> address,
  auto&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_lazy_scope(tracer, address, [&] {
    return std::format("offset(0x{:X})", item.offset);
  });
  return traced_advance(base, item.offset, tracer);
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<member_type_t<M>>
[[nodiscard]] ReadCursor<MemoryReader> read_layout_item(
  Field<M> item,
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  pointer_type_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  auto& field = target.*M;
  return read_remote_into(reader, address, field, tracer);
}

template <typename T, IsMemoryReader MemoryReader, IsTracer Tracer>
  requires CanStoreAddressOf<T, MemoryReader>
[[nodiscard]] ReadCursor<MemoryReader> read_address_into(
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  pointer_type_t<MemoryReader> ptr{};
  if (!read_bytes(reader, address, ptr, tracer)) return {};
  // static_cast safe by CanStoreAddressOf
  target = static_cast<T>(ptr);
  return traced_advance(address, sizeof(ptr), tracer);
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryReader> read_layout_item(
  Ptr<M>,
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  pointer_type_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  return read_address_into(reader, address, target.*M, tracer);
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<member_type_t<M>>
[[nodiscard]] ReadCursor<MemoryReader> read_layout_item(
  Ref<M>,
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  pointer_type_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  pointer_type_t<MemoryReader> target_ptr{};
  auto result = read_address_into(reader, address, target_ptr, tracer);
  if (!result) return {};
  if (target_ptr) {
    // ignore output: errors reported already, no need for extra error message
    std::ignore = read_remote_into(reader, target_ptr, target.*M, tracer);
  } else {
    tracer.error("null pointer");
  }
  return result;
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<optional_value_type_t<M>>
[[nodiscard]] ReadCursor<MemoryReader> read_layout_item(
  NullableRef<M> item,
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  pointer_type_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  pointer_type_t<MemoryReader> target_ptr{};
  auto result = read_address_into(reader, address, target_ptr, tracer);
  if (!result) return {};
  auto& field = target.*M;
  field.reset();
  if (target_ptr) {
    auto& target_value = field.emplace();
    // ignore output: errors reported already, no need for extra error message
    std::ignore = read_remote_into(reader, target_ptr, target_value, tracer);
    // note: keep field emplaced even if read fails, consistent with Ref
  }
  // note: null target_ptr is ok, no error reported
  return result;
}

template <
  IsLayoutItem... Items,
  IsMemoryReader MemoryReader,
  IsReadable T,
  IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryReader> read_layout(
  Layout<Items...>,
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  T& target,
  Tracer& tracer
) {
  ReadCursor<MemoryReader> result{base};
  // - ((expr), ...) is intentional: comma has the lowest precedence
  // - comma operator sequences the effects from left to right
  // - && short-circuits the evaluation so we stop on first failure
  // - Items{} is just a tag to select the overload, construction costs nothing
  ((
     result
     && (result = read_layout_item(Items{}, reader, base, result.value(), target, tracer))
   ),
   ...);
  return result;
}

}  // namespace detail

// ============================================================
// Public API: read
// ============================================================

/**
 * @brief Reads data from remote memory into a native object based on a
 * specified layout.
 *
 * @tparam MemoryReader The type for the reader callback.
 * @tparam T          The native type to deserialize into.
 * @param memory The memory abstraction providing the `MemoryReader` function.
 * @param base   The remote address to start reading from.
 * @param target The native object to populate.
 * @return Updated remote pointer after reading, as returned by `MemoryReader`.
 */
template <IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer = NoTracer>
[[nodiscard]] ReadCursor<MemoryReader> read_remote_into(
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  T& target,
  Tracer& tracer = []() -> NoTracer& {
    static NoTracer n;
    return n;
  }()
) {
  if constexpr (HasLayout<T>) {
    return detail::read_layout(layout_of_t<T>{}, reader, base, target, tracer);
  } else {
    if (!detail::read_bytes(reader, base, target, tracer)) return {};
    return traced_advance(base, sizeof(target), tracer);
  }
}

}  // namespace mempeep

// ============================================================
// Example
// ============================================================

#ifndef _MSC_VER

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wliteral-suffix"
#pragma GCC diagnostic ignored "-Wdeprecated-literal-operator"

constexpr std::int32_t operator"" i32(unsigned long long v) {
  return v <= std::numeric_limits<std::int32_t>::max()
           ? static_cast<std::int32_t>(v)
           : throw "i32 literal out of range";
}

constexpr std::int16_t operator"" i16(unsigned long long v) {
  return v <= std::numeric_limits<std::int16_t>::max()
           ? static_cast<std::int16_t>(v)
           : throw "i16 literal out of range";
}

#pragma GCC diagnostic pop

#endif

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
  using pointer_type = uint16_t;

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
  reader.write<18>(123i32);  // health
  reader.write<26>(11i32);   // pos.x
  reader.write<34>(22i32);   // pos.y
  reader.write<42>(0i16);    // target_ptr (optional)
  reader.write<44>(2i16);    // shop_ptr (optional)
  reader.write<46>(6i16);    // weapon_ptr
  reader.write<48>(60i16);   // prev_pos ref
  reader.write<50>(80i16);   // tagged_pos ref (optional)
  reader.write<52>(0i16);    // house_pos ref (optional)
  reader.write<54>(47i32);   // mana
  reader.write<60>(88i32);   // prev_pos.x
  reader.write<68>(99i32);   // prev_pos.y
  reader.write<80>(55i32);   // tagged_pos.x
  reader.write<88>(66i32);   // tagged_pos.y

  PrintTracer tracer{};
  {
    Game game{};
    assert(mempeep::read_remote_into(reader, 4i16, game, tracer));
    assert_game(game);
  }
  {
    Game game{};
    assert(mempeep::read_remote_into(reader, 4i16, game));
    assert_game(game);
  }
}
