#include <cassert>      // assert
#include <cstdint>      // std::intptr_t, ...
#include <cstring>      // std::memcpy
#include <functional>   // std::function
#include <optional>     // std::optional
#include <type_traits>  // std::same_as, ...

namespace mempeep {

// ============================================================
// Public API: Layout, RegisterLayout, ReadMemory
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

template <auto M, typename T>
concept IsMemberTypeSame = std::is_same_v<member_type_t<M>, T>;

// helper to detect std::optional
template <typename T>
struct is_optional : std::false_type {};

template <typename U>
struct is_optional<std::optional<U>> : std::true_type {};

template <typename T>
concept IsOptional = is_optional<T>::value;

template <auto M>
concept IsMemberTypeOptional = IsOptional<member_type_t<M>>;

template <auto M>
  requires IsMemberTypeOptional<M>
struct member_optional_traits;

template <typename C, typename U, std::optional<U> C::* M>
struct member_optional_traits<M> {
  using member_optional_type = U;
};

template <auto M>
  requires IsMemberTypeOptional<M>
using member_optional_type_t =
  typename member_optional_traits<M>::member_optional_type;

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
concept HasNativeLayout
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
  requires HasNativeLayout<T>
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
 * @brief Does T have a standard layout (and no custom layout)?
 */
template <typename T>
concept HasNoRegisteredLayout = HasNativeLayout<T> && !HasRegisteredLayout<T>;

template <auto M>
concept IsMemberTypeNativeLayout = HasNativeLayout<member_type_t<M>>;

template <auto M>
concept IsMemberOptionalTypeNativeLayout
  = HasNativeLayout<member_optional_type_t<M>>;

/**
 * @brief Represents a specific member (field) of a struct/class.
 * @tparam M The native field to deserialize into.
 *
 * Usage:
 *   Field<&Class::member>
 */
template <auto M>
  requires IsMemberTypeNativeLayout<M>
struct Field : LayoutItem {};

// Any signed integer type that does not exceed native pointer width.
// Signed so subtracting pointers is safe.
template <typename T>
concept IsPointerType = std::is_integral_v<T> && std::is_signed_v<T>
                        && (sizeof(T) <= sizeof(intptr_t));

template <auto M>
concept IsMemberTypePointerType = IsPointerType<member_type_t<M>>;

/**
 * @brief Padding bytes to relative to the current position in the layout.
 */
template <auto N>
  requires IsPointerType<decltype(N)> && (N > 0)
struct Pad : LayoutItem {};

/**
 * @brief Offset (in bytes) relative to the base position of the layout.
 */
template <auto N>
  requires IsPointerType<decltype(N)> && (N > 0)
struct Offset : LayoutItem {};

/**
 * @brief A pointer that is guaranteed to be valid.
 * @tparam M The native field to deserialize the pointee into.
 */
template <auto M>
  requires IsMemberTypeNativeLayout<M>
struct FieldRef : LayoutItem {};

/**
 * @brief A pointer that may be invalid.
 * @tparam M The native field to deserialize the pointee into.
 *                   Must have type std::optional<T>.
 */
template <auto M>
  requires IsMemberOptionalTypeNativeLayout<M>
struct FieldOptionalRef : LayoutItem {};

/**
 * @brief Reads a block of memory from a remote source.
 *
 * This user-implemented function must validate pointers, handle overflows,
 * and manage read errors. On failure (invalid cursor, read error, etc.), it
 * must return 0 or may throw an exception. The library performs no validation
 * and repeatedly calls this function according to the memory layout. The
 * function must handle `size == 0` or `buffer == nullptr` for pointer
 * validation: if the range `[cursor, cursor + size)` is invalid, return 0;
 * if valid but `buffer == nullptr`, return the `cursor + size`. Otherwise,
 * copy `size` bytes into `buffer` and return `cursor + size`.
 *
 * @param cursor Remote source pointer.
 * @param size   Number of bytes to read or 0 for validation.
 * @param buffer Native destination buffer or `nullptr` for validation.
 * @return       `cursor + size` on success or 0 on failure.
 */
template <typename MemoryRead, typename PointerType>
concept IsMemoryRead
  = IsPointerType<PointerType>
    && requires(
      MemoryRead memory_read, PointerType cursor, PointerType size, void* buffer
    ) {
         { memory_read(cursor, size, buffer) } -> std::same_as<PointerType>;
       };

template <
  IsPointerType PointerType,
  IsMemoryRead<PointerType> MemoryRead,
  HasNoRegisteredLayout T>
[[nodiscard]] PointerType read(
  const MemoryRead& memory_read, PointerType base, T& target
) {
  return memory_read(base, sizeof(target), &target);
};

// Forward declaration to support recursive reading.
template <
  IsPointerType PointerType,
  IsMemoryRead<PointerType> MemoryRead,
  HasRegisteredLayout T>
[[nodiscard]] PointerType read(
  const MemoryRead& memory_read, PointerType base, T& target
);

namespace detail {

// ============================================================
// Helpers
// ============================================================

template <
  IsPointerType PointerType,
  IsMemoryRead<PointerType> MemoryRead,
  HasNativeLayout T,
  PointerType N>
  requires(N > 0)
[[nodiscard]] PointerType read_layout_item(
  Pad<N>, const MemoryRead& memory_read, PointerType, PointerType cursor, T&
) {
  return memory_read(cursor, N, nullptr);
}

template <
  IsPointerType PointerType,
  IsMemoryRead<PointerType> MemoryRead,
  HasNativeLayout T,
  PointerType N>
  requires(N > 0)
[[nodiscard]] PointerType read_layout_item(
  Offset<N>, const MemoryRead& memory_read, PointerType base, PointerType, T&
) {
  return memory_read(base, N, nullptr);
}

template <
  auto M,
  IsPointerType PointerType,
  IsMemoryRead<PointerType> MemoryRead,
  HasNativeLayout T>
  requires IsMemberTypeNativeLayout<M>
[[nodiscard]] PointerType read_layout_item(
  Field<M>,
  const MemoryRead& memory_read,
  PointerType base,
  PointerType cursor,
  T& target
) {
  auto& field = target.*M;
  return read(memory_read, cursor, field);
}

template <
  auto M,
  IsPointerType PointerType,
  IsMemoryRead<PointerType> MemoryRead,
  HasNativeLayout T>
  requires IsMemberTypeNativeLayout<M>
[[nodiscard]] PointerType read_layout_item(
  FieldRef<M>,
  const MemoryRead& memory_read,
  PointerType base,
  PointerType cursor,
  T& target
) {
  PointerType target_ptr{};
  cursor = memory_read(cursor, sizeof(target_ptr), &target_ptr);
  auto& field = target.*M;
  if (target_ptr) {
    if (!read(memory_read, target_ptr, field)) {
      // TODO handle error
    }
  }
  return cursor;
}

template <
  auto M,
  IsPointerType PointerType,
  IsMemoryRead<PointerType> MemoryRead,
  HasNativeLayout T>
  requires IsMemberOptionalTypeNativeLayout<M>
[[nodiscard]] PointerType read_layout_item(
  FieldOptionalRef<M>,
  const MemoryRead& memory_read,
  PointerType base,
  PointerType cursor,
  T& target
) {
  PointerType target_ptr{};
  cursor = memory_read(cursor, sizeof(target_ptr), &target_ptr);
  auto& field = target.*M;
  if (target_ptr) {
    using U = member_optional_type_t<M>;  // std::optional<U> -> U
    U value{};
    if (read(memory_read, target_ptr, value)) {
      field = std::move(value);
    }
  } else {
    field.reset();  // target_ptr == 0 so field must be {}
  }
  return cursor;
}

template <
  IsLayoutItem... Items,
  IsPointerType PointerType,
  IsMemoryRead<PointerType> MemoryRead,
  HasNativeLayout T>
[[nodiscard]] PointerType read_layout(
  Layout<Items...>, const MemoryRead& memory_read, PointerType base, T& target
) {
  PointerType cursor = base;
  // fold from first to last item
  ((cursor = read_layout_item(Items{}, memory_read, base, cursor, target)),
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
 * This function does not perform any validation or error checking.
 * It simply calls the user-provided `MemoryRead` function repeatedly.
 * It is mandatory that the `MemoryRead` implementation performs all
 * validation and error handling.
 * If `MemoryRead` reports failure by returning a sentinel like 0,
 * it must be prepared to handle such sentinel cursor values appropriately.
 *
 * @tparam SizeOfPtr Size of remote pointers (4 for 32-bit, 8 for 64-bit).
 * @tparam T The native type to deserialize into.
 * @param memory The memory abstraction providing the `MemoryRead` function.
 * @param cursor The current remote memory pointer.
 * @param target The native object to populate.
 * @return Updated remote pointer after reading, as returned by `MemoryRead`.
 */
template <
  IsPointerType PointerType,
  IsMemoryRead<PointerType> MemoryRead,
  HasRegisteredLayout T>
[[nodiscard]] PointerType read(
  const MemoryRead& memory_read, PointerType base, T& target
) {
  return detail::read_layout(
    registered_layout_t<T>{}, memory_read, base, target
  );
}

}  // namespace mempeep

// ============================================================
// Example
// ============================================================

#include <iostream>

// example with 16 bit pointers, for fun
template <int16_t N>
  requires(N > 0)
struct MemoryReadMock {
  std::byte data[N]{};

  int16_t operator()(int16_t cursor, int16_t size, void* buffer) const {
    // handle overflow/underflow (note: cursor 0 is not valid)
    std::cout << "read: " << cursor << " " << size << std::endl;
    if (!(size >= 0 && size <= N && cursor >= 1 && cursor <= N - size)) {
      std::cerr << "error" << std::endl;
      return 0;
    }
    if (buffer && size) std::memcpy(buffer, data + cursor, size);
    return cursor + size;
  }

  template <typename T>
  void write(int16_t offset, T value) {
    assert(sizeof(value) <= N && offset >= 1 && offset <= N - sizeof(value));
    std::memcpy(data + offset, &value, sizeof(value));
  }
};

struct Pos {
  int32_t x, y;
};

struct Player {
  int32_t health;
  Pos pos;
  int16_t target_ptr;
  int16_t shop_ptr;
  int16_t weapon_ptr;
  Pos prev_pos;
  std::optional<Pos> tagged_pos;
  std::optional<Pos> house_pos;
  int32_t mana;
};

template <>
struct mempeep::RegisterLayout<Pos> {
  using layout
    = Layout<Field<&Pos::x>, Pad<int16_t(4)>, Field<&Pos::y>, Pad<int16_t(4)>>;
};

template <>
struct mempeep::RegisterLayout<Player> {
  using layout = Layout<
    Offset<int16_t(8)>,
    Field<&Player::health>,
    Offset<int16_t(16)>,
    Field<&Player::pos>,
    Field<&Player::target_ptr>,
    Field<&Player::shop_ptr>,
    Field<&Player::weapon_ptr>,
    FieldRef<&Player::prev_pos>,
    FieldOptionalRef<&Player::tagged_pos>,
    FieldOptionalRef<&Player::house_pos>,
    Field<&Player::mana>>;
};

int main() {
  MemoryReadMock<128> memory_read{};
  memory_read.write(18, int32_t(123));  // health
  memory_read.write(26, int32_t(11));   // pos.x
  memory_read.write(34, int32_t(22));   // pos.y
  memory_read.write(42, int16_t(0));    // target_ptr (optional)
  memory_read.write(44, int16_t(2));    // shop_ptr (optional)
  memory_read.write(46, int16_t(6));    // weapon_ptr
  memory_read.write(48, int16_t(60));   // prev_pos ref
  memory_read.write(50, int16_t(80));   // tagged_pos ref (optional)
  memory_read.write(52, int32_t(0));    // house_pos ref (optional)
  memory_read.write(54, int32_t(47));   // mana
  memory_read.write(60, int32_t(88));   // prev_pos.x
  memory_read.write(68, int32_t(99));   // prev_pos.y
  memory_read.write(80, int32_t(55));   // tagged_pos.x
  memory_read.write(88, int32_t(66));   // tagged_pos.y

  Player player{};
  assert(mempeep::read(memory_read, int16_t(10), player));

  assert(player.health == 123);
  assert(player.pos.x == 11);
  assert(player.pos.y == 22);
  assert(player.target_ptr == 0);
  assert(player.shop_ptr == 2);
  assert(player.weapon_ptr == 6);
  assert(player.prev_pos.x == 88);
  assert(player.prev_pos.y == 99);
  assert(player.mana == 47);
  assert(player.tagged_pos.has_value());
  assert(player.tagged_pos->x == 55);
  assert(player.tagged_pos->y == 66);
  assert(!player.house_pos.has_value());
}