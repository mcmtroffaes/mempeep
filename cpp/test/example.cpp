#include <cassert>      // assert
#include <cstring>      // std::memcpy
#include <format>       // std::format
#include <functional>   // std::function
#include <optional>     // std::optional
#include <type_traits>  // std::same_as, ...
#include <utility>      // std::cmp_less_equal

namespace mempeep {

// ============================================================
// Integer concepts
// ============================================================

// Shorthand.
template <typename T>
concept IsInteger
  = std::is_integral_v<T> && !std::same_as<T, bool> && !std::same_as<T, char>;

namespace detail {

template <IsInteger T>
struct TypeRange {
  static constexpr auto min = std::numeric_limits<T>::lowest();
  static constexpr auto max = std::numeric_limits<T>::max();
};

template <IsInteger auto N>
struct ValueRange {
  static constexpr auto min = N;
  static constexpr auto max = N;
};

// check FromRange is inside ToRange
// i.e. ToRange::min <= FromRange::min && FromRange::max <= ToRange::max
// note: use std::cmp_less_equal to avoid signed/unsigned comparison pitfalls
template <typename FromRange, typename ToRange>
concept IsInRange = std::cmp_less_equal(ToRange::min, FromRange::min)
                    && std::cmp_less_equal(FromRange::max, ToRange::max);

}  // namespace detail

template <typename From, typename To>
concept IsTypeRepresentableAs
  = detail::IsInRange<detail::TypeRange<From>, detail::TypeRange<To>>;

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

template <typename T>
concept IsOptional = is_optional<T>::value;

template <auto M>
  requires IsOptional<member_type_t<M>>
struct member_optional_traits;

template <typename C, typename U, std::optional<U> C::* M>
struct member_optional_traits<M> {
  using optional_value_type = U;
};

template <auto M>
  requires IsOptional<member_type_t<M>>
using optional_value_type_t =
  typename member_optional_traits<M>::optional_value_type;

// ============================================================
// Tracing
// ============================================================

// note: uint64_t address avoids dependence on MemoryRead
// this is ok for logging, and keeps implementation simple
template <typename Tracer>
concept IsTracer = requires(
  Tracer& tracer,
  std::uint64_t address,
  std::string_view label,
  std::string_view reason
) {
  typename Tracer::Scope;
  { Tracer::Scope(tracer, address, label) };
  { tracer.error(reason) } -> std::same_as<void>;
};

// deduces Tracer::Scope from Tracer, avoiding repetition at call sites
template <IsTracer Tracer, IsInteger N>
auto make_scope(Tracer& tracer, N address, std::string_view label) {
  return
    typename Tracer::Scope(tracer, static_cast<std::uint64_t>(address), label);
}

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

struct TestMemberName {
  int the_member;
};

static_assert(member_name<&TestMemberName::the_member>() == "the_member");

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
 * @brief Template for registering the remote layout of a native struct.
 *
 * To be specialized by the user for each struct with non-native remote layout.
 * Example:
 *
 *   template <>
 *   struct RegisterLayout<Pos> {
 *     using layout = Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>>;
 *   };
 *
 * @tparam T Native struct to register the layout for.
 */
template <typename T>
  requires IsReadable<T>
struct RegisterLayout;

/**
 * @brief Does T have a custom layout?
 *
 * Checks if RegisterLayout<T>::layout exists.
 */
template <typename T>
concept HasRegisteredLayout = requires { typename RegisterLayout<T>::layout; };

/**
 * @brief Shorthand for RegisterLayout<T>::layout.
 */
template <typename T>
  requires HasRegisteredLayout<T>
using registered_layout_t = typename RegisterLayout<T>::layout;

/**
 * @brief A field. Its type can have any native layout.
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
 * @tparam N Number of bytes.
 *           Its value must be representable by pointer_type_t<MemoryRead>.
 *           The read template will not instantiate otherwise.
 */
template <IsInteger auto N>
struct Pad : LayoutItem {};

/**
 * @brief Absolute offset relative to base position of the layout.
 * @tparam N The offset (in bytes).
 *           Its value must be representable by pointer_type_t<MemoryRead>.
 *           The read template will not instantiate otherwise.
 */
template <IsInteger auto N>
struct Seek : LayoutItem {};

/**
 * @brief Raw address, not followed.
 * @tparam M The native field to deserialize the address into.
 *           Its type must be wide enough to hold pointer_type_t<MemoryRead>.
 *           The read template will not instantiate otherwise.
 */
template <auto M>
  requires IsInteger<member_type_t<M>>
struct FieldAddr : LayoutItem {};

/**
 * @brief Follow address and read pointee (null is an error).
 * @tparam M The native field to deserialize the pointee into.
 */
template <auto M>
  requires IsReadable<member_type_t<M>>
struct FieldDeref : LayoutItem {};

/**
 * @brief Follow address and read pointee if not null. Stored as optional.
 * @tparam M The native field to deserialize the pointee into.
 *           Must have type std::optional<T>.
 */
template <auto M>
  requires IsReadable<optional_value_type_t<M>>
struct FieldDerefOpt : LayoutItem {};

/**
 * @brief Extract pointer_type from MemoryRead.
 */
template <typename MemoryRead>
using pointer_type_t = typename MemoryRead::pointer_type;

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
template <typename MemoryRead>
concept IsMemoryRead = requires(
  MemoryRead memory_read,
  pointer_type_t<MemoryRead> address,
  std::size_t size,
  void* buffer
) {
  { memory_read(address, size, buffer) } -> std::same_as<bool>;
};

// Stores result of reading: address after reading, or null if read failed.
// Note error messages are propagated separately through the tracer.
template <IsMemoryRead MemoryRead>
using ReadCursor = std::optional<pointer_type_t<MemoryRead>>;

// Addition with overflow check, handling mixed types.
template <IsInteger PointerType, IsInteger M, IsTracer Tracer>
[[nodiscard]] std::optional<PointerType> safe_offset(
  PointerType address, M offset, Tracer& tracer
) {
  using Limits = std::numeric_limits<PointerType>;
  constexpr PointerType min = Limits::min();
  constexpr PointerType max = Limits::max();
  if (!std::in_range<PointerType>(offset)) {
    tracer.error("offset out of range");
    return {};
  }
  PointerType ofs = static_cast<PointerType>(offset);
  // safely check min <= address + ofs <= max
  if (ofs > 0) {
    if (address > max - ofs) {
      tracer.error("offset overflow");
      return {};
    }
  } else if (ofs < 0) {
    if (address < min - ofs) {
      tracer.error("offset underflow");
      return {};
    }
  }
  return static_cast<PointerType>(address + ofs);
}

// Forward declaration to support recursive reading.
template <IsMemoryRead MemoryRead, IsReadable T, IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryRead> read(
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  T& target,
  Tracer& tracer
);

namespace detail {

// ============================================================
// Helpers
// ============================================================

template <
  IsInteger auto N,
  IsMemoryRead MemoryRead,
  IsReadable T,
  IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryRead> read_layout_item(
  Pad<N> item,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  pointer_type_t<MemoryRead> address,
  T&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope
    = make_scope(tracer, address, std::format("pad(0x{:X})", N));
  return safe_offset(address, N, tracer);
}

template <
  IsInteger auto N,
  IsMemoryRead MemoryRead,
  IsReadable T,
  IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryRead> read_layout_item(
  Seek<N> item,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  pointer_type_t<MemoryRead> address,
  T&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope
    = make_scope(tracer, address, std::format("offset(0x{:X})", N));
  return safe_offset(base, N, tracer);
}

template <auto M, IsMemoryRead MemoryRead, IsReadable T, IsTracer Tracer>
  requires IsReadable<member_type_t<M>>
[[nodiscard]] ReadCursor<MemoryRead> read_layout_item(
  Field<M> item,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  pointer_type_t<MemoryRead> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  auto& field = target.*M;
  return read(memory_read, address, field, tracer);
}

template <auto M, IsMemoryRead MemoryRead, IsReadable T, IsTracer Tracer>
  requires IsTypeRepresentableAs<pointer_type_t<MemoryRead>, member_type_t<M>>
[[nodiscard]] ReadCursor<MemoryRead> read_layout_item(
  FieldAddr<M> item,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  pointer_type_t<MemoryRead> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  pointer_type_t<MemoryRead> target_ptr{};
  if (!memory_read(address, sizeof(target_ptr), &target_ptr)) return {};
  auto& field = target.*M;
  // static_cast safe by requires IsTypeRepresentableAs
  field = static_cast<member_type_t<M>>(target_ptr);
  return safe_offset(address, sizeof(target_ptr), tracer);
}

template <auto M, IsMemoryRead MemoryRead, IsReadable T, IsTracer Tracer>
  requires IsReadable<member_type_t<M>>
[[nodiscard]] ReadCursor<MemoryRead> read_layout_item(
  FieldDeref<M> item,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  pointer_type_t<MemoryRead> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  pointer_type_t<MemoryRead> target_ptr{};
  if (!memory_read(address, sizeof(target_ptr), &target_ptr)) return {};
  if (target_ptr) {
    auto& field = target.*M;
    if (!read(memory_read, target_ptr, field, tracer)) {
      tracer.error("failed to read pointee");
    }
  } else {
    tracer.error("null pointer");
  }
  return safe_offset(address, sizeof(target_ptr), tracer);
}

template <auto M, IsMemoryRead MemoryRead, IsReadable T, IsTracer Tracer>
  requires IsReadable<optional_value_type_t<M>>
[[nodiscard]] ReadCursor<MemoryRead> read_layout_item(
  FieldDerefOpt<M> item,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  pointer_type_t<MemoryRead> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  pointer_type_t<MemoryRead> target_ptr{};
  if (!memory_read(address, sizeof(target_ptr), &target_ptr)) return {};
  auto& field = target.*M;
  field.reset();
  if (target_ptr) {
    using U = optional_value_type_t<M>;  // std::optional<U> -> U
    U value{};
    if (read(memory_read, target_ptr, value, tracer)) {
      field = std::move(value);
    } else {
      tracer.error("failed to read pointee");
    }
  }
  return safe_offset(address, sizeof(target_ptr), tracer);
}

template <
  IsLayoutItem... Items,
  IsMemoryRead MemoryRead,
  IsReadable T,
  IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryRead> read_layout(
  Layout<Items...>,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  T& target,
  Tracer& tracer
) {
  ReadCursor<MemoryRead> result{base};
  // - ((expr), ...) is intentional: comma has the lowest precedence
  // - comma operator sequences the effects from left to right
  // - && short-circuits the evaluation so we stop on first failure
  // - Items{} is just a tag to select the overload, construction costs nothing
  ((
     result
     && (result = read_layout_item(Items{}, memory_read, base, result.value(), target, tracer))
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
 * @tparam MemoryRead The type for the memory_read callback.
 * @tparam T          The native type to deserialize into.
 * @param memory The memory abstraction providing the `MemoryRead` function.
 * @param base   The remote address to start reading from.
 * @param target The native object to populate.
 * @return Updated remote pointer after reading, as returned by `MemoryRead`.
 */
template <IsMemoryRead MemoryRead, IsReadable T, IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryRead> read(
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  T& target,
  Tracer& tracer
) {
  if constexpr (HasRegisteredLayout<T>) {
    return detail::read_layout(
      registered_layout_t<T>{}, memory_read, base, target, tracer
    );
  } else {
    if (!memory_read(base, sizeof(target), &target)) return {};
    return safe_offset(base, sizeof(target), tracer);
  }
}

}  // namespace mempeep

// ============================================================
// Example
// ============================================================

#include <cstdint>   // std::int32_t, ...
#include <iostream>  // std::cout, ...

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

struct SimpleTracer {
  int indent = 0;
  int64_t address = 0;

  void error(std::string_view reason) {
    auto whitespace = std::string(indent, ' ');
    std::cout << std::format("[{:08X}] ", address) << whitespace << whitespace
              << reason << std::endl;
  }

  struct Scope {
    SimpleTracer& t;

    Scope(SimpleTracer& _t, int64_t address, std::string_view label) : t(_t) {
      t.address = address;
      t.error(label);
      t.indent++;
    }

    ~Scope() { t.indent--; }
  };
};

// example with 16 bit pointers, for fun
template <int16_t BASE, int16_t N>
  requires(BASE > 0 && N > 0)
struct MemoryReadMock {
  SimpleTracer& t;
  using pointer_type = int16_t;
  std::byte data[N]{};

  bool operator()(int16_t address, std::size_t size, void* buffer) const {
    // handle overflow/underflow
    // note: static_cast is safe, size <= N means it fits
    if (!(buffer && size > 0 && size <= N && address >= BASE
          && address - BASE <= N - static_cast<int16_t>(size))) {
      t.error("read error");
      return false;
    }
    std::memcpy(buffer, data + (address - BASE), size);
    return true;
  }

  template <typename T>
  void write(int16_t offset, T value) {
    assert(
      sizeof(value) <= N && offset >= BASE && offset - BASE <= N - sizeof(value)
    );
    std::memcpy(data + (offset - BASE), &value, sizeof(value));
  }
};

struct Pos {
  int32_t x, y;
};

struct Player {
  int32_t health;
  Pos pos;
  int16_t target_ptr;
  int32_t shop_ptr;  // wider than needed, still fine
  int16_t weapon_ptr;
  Pos prev_pos;
  std::optional<Pos> tagged_pos;
  std::optional<Pos> house_pos;
  int32_t mana;
};

struct Game {
  Player player;
};

// intentionally have 4 padding bytes at end, for testing
template <>
struct mempeep::RegisterLayout<Pos> {
  using layout = Layout<Field<&Pos::x>, Pad<4i16>, Field<&Pos::y>, Pad<4i16>>;
};

template <>
struct mempeep::RegisterLayout<Player> {
  using layout = Layout<
    Seek<8>,
    Field<&Player::health>,
    Seek<16>,
    Field<&Player::pos>,
    FieldAddr<&Player::target_ptr>,
    FieldAddr<&Player::shop_ptr>,
    FieldAddr<&Player::weapon_ptr>,
    FieldDeref<&Player::prev_pos>,
    FieldDerefOpt<&Player::tagged_pos>,
    FieldDerefOpt<&Player::house_pos>,
    Field<&Player::mana>>;
};

template <>
struct mempeep::RegisterLayout<Game> {
  using layout = Layout<Seek<6>, Field<&Game::player>>;
};

int main() {
  SimpleTracer tracer{};
  MemoryReadMock<1, 128> memory_read{tracer};
  memory_read.write(18, 123i32);  // health
  memory_read.write(26, 11i32);   // pos.x
  memory_read.write(34, 22i32);   // pos.y
  memory_read.write(42, 0i16);    // target_ptr (optional)
  memory_read.write(44, 2i16);    // shop_ptr (optional)
  memory_read.write(46, 6i16);    // weapon_ptr
  memory_read.write(48, 60i16);   // prev_pos ref
  memory_read.write(50, 80i16);   // tagged_pos ref (optional)
  memory_read.write(52, 0i32);    // house_pos ref (optional)
  memory_read.write(54, 47i32);   // mana
  memory_read.write(60, 88i32);   // prev_pos.x
  memory_read.write(68, 99i32);   // prev_pos.y
  memory_read.write(80, 55i32);   // tagged_pos.x
  memory_read.write(88, 66i32);   // tagged_pos.y

  Game game{};
  assert(mempeep::read(memory_read, 4i16, game, tracer));

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
