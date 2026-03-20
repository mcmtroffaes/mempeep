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
concept IsInteger = std::is_integral_v<T>;

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

template <auto N, typename T>
concept IsValueInRangeFor
  = detail::IsInRange<detail::ValueRange<N>, detail::TypeRange<T>>;

template <typename From, typename To>
concept IsTypeInRangeFor
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

template <typename Tracer, typename Item>
concept HasScopeFor = requires(Tracer tracer, Item item) {
  typename Tracer::template Scope<Item>;
  { typename Tracer::template Scope<Item>(tracer, item) };
};

// helper function for peak C++ syntax
template <typename Tracer, typename Item>
  requires HasScopeFor<Tracer, Item>
auto make_scope(Tracer& tracer, const Item& item) {
  return typename Tracer::template Scope<Item>(tracer, item);
}

// TODO logging

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
 * Each item must be a Field, Pad, or Offset.
 * Example:
 *
 *   Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>>
 *
 * @tparam Items Sequence of Field, Offset, and Pad types.
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
 * Valid items in a layout are Field<&Class::member>, Pad<N>, and Offset<N>.
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
 * @brief Padding bytes to relative to the current position in the layout.
 * @tparam N Number of bytes.
 *           Its value must be representable by pointer_type_t<MemoryRead>.
 *           The read template will not instantiate otherwise.
 */
template <IsInteger auto N>
struct Pad : LayoutItem {};

/**
 * @brief Offset (in bytes) relative to the base position of the layout.
 * @tparam N The offset.
 *           Its value must be representable by pointer_type_t<MemoryRead>.
 *           The read template will not instantiate otherwise.
 */
template <IsInteger auto N>
struct Offset : LayoutItem {};

/**
 * @brief A raw pointer (not dereferenced or checked otherwise).
 * @tparam M The native field to deserialize the pointer into.
 *           Its type must be wide enough to hold pointer_type_t<MemoryRead>.
 *           The read template will not instantiate otherwise.
 */
template <auto M>
  requires IsInteger<member_type_t<M>>
struct FieldPtr : LayoutItem {};

/**
 * @brief A pointer that can always be dereferenced.
 * @tparam M The native field to deserialize the pointee into.
 */
template <auto M>
  requires IsReadable<member_type_t<M>>
struct FieldRef : LayoutItem {};

/**
 * @brief A pointer that may be invalid.
 * @tparam M The native field to deserialize the pointee into.
 *                   Must have type std::optional<T>.
 */
template <auto M>
  requires IsReadable<optional_value_type_t<M>>
struct FieldOptionalRef : LayoutItem {};

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
  pointer_type_t<MemoryRead> size,
  void* buffer
) {
  { memory_read(address, size, buffer) } -> std::same_as<bool>;
};

template <IsMemoryRead MemoryRead>
using Cursor = std::optional<pointer_type_t<MemoryRead>>;

// Addition with overflow check, handling mixed types.
template <IsInteger PointerType, IsInteger M>
[[nodiscard]] std::optional<PointerType> safe_offset(
  PointerType address, M offset
) {
  using Limits = std::numeric_limits<PointerType>;
  constexpr PointerType min = Limits::min();
  constexpr PointerType max = Limits::max();
  if (!std::in_range<PointerType>(offset)) return {};
  PointerType ofs = static_cast<PointerType>(offset);
  // safely check min <= address + ofs <= max
  if (ofs > 0) {
    if (address > max - ofs) return {};
  } else if (ofs < 0) {
    if (address < min - ofs) return {};
  }
  return static_cast<PointerType>(address + ofs);
}

// Forward declaration to support recursive reading.
template <IsMemoryRead MemoryRead, IsReadable T, typename Tracer>
[[nodiscard]] Cursor<MemoryRead> read(
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  T& target,
  Tracer& tracer
);

namespace detail {

// ============================================================
// Helpers
// ============================================================

template <IsMemoryRead MemoryRead>
struct LayoutPointers {
  pointer_type_t<MemoryRead> base;
  pointer_type_t<MemoryRead> cursor;
};

template <IsInteger auto N, IsMemoryRead MemoryRead, IsReadable T, typename Tracer>
  requires HasScopeFor<Tracer, Pad<N>>
[[nodiscard]] Cursor<MemoryRead> read_layout_item(
  Pad<N> item,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  pointer_type_t<MemoryRead> address,
  T&,
  Tracer& tracer
) {
  auto scope = make_scope(tracer, item);
  return safe_offset(address, N);
}

template <IsInteger auto N, IsMemoryRead MemoryRead, IsReadable T, typename Tracer>
  requires HasScopeFor<Tracer, Offset<N>>
[[nodiscard]] Cursor<MemoryRead> read_layout_item(
  Offset<N> item,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  pointer_type_t<MemoryRead> address,
  T&,
  Tracer& tracer
) {
  auto scope = make_scope(tracer, item);
  return safe_offset(base, N);
}

template <auto M, IsMemoryRead MemoryRead, IsReadable T, typename Tracer>
  requires IsReadable<member_type_t<M>> && HasScopeFor<Tracer, Field<M>>
[[nodiscard]] Cursor<MemoryRead> read_layout_item(
  Field<M> item,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  pointer_type_t<MemoryRead> address,
  T& target,
  Tracer& tracer
) {
  auto scope = make_scope(tracer, item);
  auto& field = target.*M;
  return read(memory_read, address, field, tracer);
}

template <auto M, IsMemoryRead MemoryRead, IsReadable T, typename Tracer>
  requires IsTypeInRangeFor<pointer_type_t<MemoryRead>, member_type_t<M>>
           && HasScopeFor<Tracer, FieldPtr<M>>
[[nodiscard]] Cursor<MemoryRead> read_layout_item(
  FieldPtr<M> item,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  pointer_type_t<MemoryRead> address,
  T& target,
  Tracer& tracer
) {
  auto scope = make_scope(tracer, item);
  pointer_type_t<MemoryRead> target_ptr{};
  if (!memory_read(address, sizeof(target_ptr), &target_ptr)) return {};
  auto& field = target.*M;
  // static_cast safe by requires IsTypeInRangeFor
  field = static_cast<member_type_t<M>>(target_ptr);
  return safe_offset(address, sizeof(target_ptr));
}

template <auto M, IsMemoryRead MemoryRead, IsReadable T, typename Tracer>
  requires IsReadable<member_type_t<M>> && HasScopeFor<Tracer, FieldRef<M>>
[[nodiscard]] Cursor<MemoryRead> read_layout_item(
  FieldRef<M> item,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  pointer_type_t<MemoryRead> address,
  T& target,
  Tracer& tracer
) {
  auto scope = make_scope(tracer, item);
  pointer_type_t<MemoryRead> target_ptr{};
  if (!memory_read(address, sizeof(target_ptr), &target_ptr)) return {};
  if (target_ptr) {
    auto& field = target.*M;
    if (!read(memory_read, target_ptr, field, tracer)) {
      // TODO handle error
    }
  }
  return safe_offset(address, sizeof(target_ptr));
}

template <auto M, IsMemoryRead MemoryRead, IsReadable T, typename Tracer>
  requires IsReadable<optional_value_type_t<M>>
           && HasScopeFor<Tracer, FieldOptionalRef<M>>
[[nodiscard]] Cursor<MemoryRead> read_layout_item(
  FieldOptionalRef<M> item,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  pointer_type_t<MemoryRead> address,
  T& target,
  Tracer& tracer
) {
  auto scope = make_scope(tracer, item);
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
      // TODO handle error
    }
  }
  return safe_offset(address, sizeof(target_ptr));
}

template <
  IsLayoutItem... Items,
  IsMemoryRead MemoryRead,
  IsReadable T,
  typename Tracer>
[[nodiscard]] Cursor<MemoryRead> read_layout(
  Layout<Items...>,
  const MemoryRead& memory_read,
  pointer_type_t<MemoryRead> base,
  T& target,
  Tracer& tracer
) {
  Cursor<MemoryRead> cursor{base};
  // fold from first to last item, only keep going as long as cursor is ok
  // note ((expr), ...) is intentional: comma operator has the lowest precedence
  ((
     cursor
     && (cursor = read_layout_item(Items{}, memory_read, base, cursor.value(), target, tracer))
   ),
   ...);
  return cursor;
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
 * @param cursor The current remote memory pointer.
 * @param target The native object to populate.
 * @return Updated remote pointer after reading, as returned by `MemoryRead`.
 */
template <IsMemoryRead MemoryRead, IsReadable T, typename Tracer>
[[nodiscard]] Cursor<MemoryRead> read(
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
    return safe_offset(base, sizeof(target));
  }
};

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

  template <typename... Args>
  void msg(std::format_string<Args...> fmt, Args&&... args) {
    std::cout << std::string(indent, ' ')
              << std::format(fmt, std::forward<Args>(args)...) << std::endl;
  }

  template <typename Item>
  struct Scope {
    SimpleTracer& t;

    Scope(SimpleTracer& _t, const Item&) : t(_t) {
      t.msg("{}", typeid(Item).name());
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

  bool operator()(int16_t address, int16_t size, void* buffer) const {
    // handle overflow/underflow
    t.msg("read: {} {}", address, address + size);
    if (!(buffer && size > 0 && size <= N && address >= BASE
          && address - BASE <= N - size)) {
      std::cerr << std::string(t.indent, ' ') << "error" << std::endl;
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

template <>
struct mempeep::RegisterLayout<Pos> {
  using layout = Layout<Field<&Pos::x>, Pad<4i16>, Field<&Pos::y>, Pad<4i16>>;
};

template <>
struct mempeep::RegisterLayout<Player> {
  using layout = Layout<
    Offset<8>,
    Field<&Player::health>,
    Offset<16>,
    Field<&Player::pos>,
    FieldPtr<&Player::target_ptr>,
    FieldPtr<&Player::shop_ptr>,
    FieldPtr<&Player::weapon_ptr>,
    FieldRef<&Player::prev_pos>,
    FieldOptionalRef<&Player::tagged_pos>,
    FieldOptionalRef<&Player::house_pos>,
    Field<&Player::mana>>;
};

template <>
struct mempeep::RegisterLayout<Game> {
  using layout = Layout<Offset<6>, Field<&Game::player>>;
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
