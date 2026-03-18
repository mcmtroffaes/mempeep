#include <cassert>      // assert
#include <cstdint>      // std::intptr_t, ...
#include <cstring>      // std::memcpy
#include <functional>   // std::function
#include <optional>     // std::optional
#include <type_traits>  // std::is_same_v, ...

namespace mempeep {

// ============================================================
// Public API: Layout, RegisterLayout, ReadMemory
// ============================================================

template <intptr_t N>
concept IsPositiveIntPtr = (N > 0);

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
struct layout_item_tag {};

template <typename T>
concept IsLayoutItem = requires {
  typename T::layout_item_tag;
} && std::same_as<typename T::layout_item_tag, layout_item_tag>;

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
template <typename T, typename... Items>
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
struct Field {
  using layout_item_tag = layout_item_tag;
};

/**
 * @brief Padding bytes to relative to the current position in the layout.
 */
template <intptr_t N>
  requires IsPositiveIntPtr<N>
struct Pad {
  using layout_item_tag = layout_item_tag;
};

/**
 * @brief Offset (in bytes) relative to the base position of the layout.
 */
template <intptr_t N>
  requires IsPositiveIntPtr<N>
struct Offset {
  using layout_item_tag = layout_item_tag;
};

/**
 * @brief A pointer that is guaranteed to be valid.
 * @tparam M The native field to deserialize the pointee into.
 */
template <auto M>
  requires IsMemberTypeNativeLayout<M>
struct FieldRef {
  using layout_item_tag = layout_item_tag;
};

/**
 * @brief A pointer that may be invalid.
 * @tparam M The native field to deserialize the pointee into.
 *                   Must have type std::optional<T>.
 */
template <auto M>
  requires IsMemberOptionalTypeNativeLayout<M>
struct FieldOptionalRef {
  using layout_item_tag = layout_item_tag;
};

/**
 * @brief Remote pointer (as an intptr_t) that is guaranteed to be valid.
 *
 * Usage:
 *   FieldPtr<&Class::member>
 */
template <auto M>
  requires IsMemberTypeSame<M, intptr_t>
struct FieldPtr {
  using layout_item_tag = layout_item_tag;
};

/**
 * @brief Remote pointer (as an intptr_t) that may be invalid.
 *
 * Usage:
 *   FieldOptionalPtr<&Class::member>
 */
template <auto M>
  requires IsMemberTypeSame<M, intptr_t>
struct FieldOptionalPtr {
  using layout_item_tag = layout_item_tag;
};

using MemoryRead = std::function<intptr_t(intptr_t, intptr_t, void*)>;

template <std::size_t N>
concept IsSupportedSizeOfInt = (N == 1 || N == 2 || N == 4 || N == 8);

template <std::size_t N>
concept IsSupportedSizeOfPtr
  = IsSupportedSizeOfInt<N> && (N <= sizeof(intptr_t));

/**
 * @brief Remote memory abstraction layer.
 *
 * Encapsulates reading as well as the size of remote pointers.
 *
 * @tparam SizeOfPtr Size of remote pointers (4 for 32-bit, 8 for 64-bit).
 */
template <std::size_t SizeOfPtr>
  requires IsSupportedSizeOfPtr<SizeOfPtr>
struct Memory {
  /**
   * @brief User-provided memory read function
   *
   * This function is responsible for reading a block of memory from a remote
   * source.
   *
   * This function must perform all necessary validation and error
   * checking internally.
   * It is the responsibility of the implementation to handle pointer
   * validation, overflow detection, and error conditions.
   * If an error occurs during reading, the function may return a sentinel
   * value (such as 0) to signal failure. In that case, future calls to this
   * function with this sentinel value as the cursor may be made, and the
   * function should be prepared to handle this appropriately. Alternatively,
   * the function may raise an exception to abort the reading process.
   * The library does not perform any validation or error checking on the
   * return value; it simply calls this function repeatedly according to the
   * specified layout.
   *
   * @param cursor Remote source pointer.
   * @param size   Number of bytes to copy (zero if just checking cursor).
   * @param buffer Native destination buffer (null if just advancing cursor).
   * @return       Updated remote pointer.
   */
  MemoryRead read;
};

template <std::size_t SizeOfPtr, HasNoRegisteredLayout T>
  requires IsSupportedSizeOfPtr<SizeOfPtr>
intptr_t read(const Memory<SizeOfPtr>& memory, intptr_t base, T& target) {
  return memory.read(base, sizeof(target), &target);
};

// Forward declaration to support recursive reading.
template <std::size_t SizeOfPtr, HasRegisteredLayout T>
  requires IsSupportedSizeOfPtr<SizeOfPtr>
intptr_t read(const Memory<SizeOfPtr>& memory, intptr_t base, T& target);

namespace detail {

// ============================================================
// Helpers
// ============================================================

template <typename T>
struct optional_traits {
  static_assert(false, "Expected std::optional<T>");
};

template <typename T>
struct optional_traits<std::optional<T>> {
  using value_type = T;
};

template <typename T>
using optional_value_t = typename optional_traits<T>::value_type;

template <std::size_t Size>
  requires IsSupportedSizeOfInt<Size>
struct signed_int;

template <>
struct signed_int<1> {
  using type = std::int8_t;
};

template <>
struct signed_int<2> {
  using type = std::int16_t;
};

template <>
struct signed_int<4> {
  using type = std::int32_t;
};

template <>
struct signed_int<8> {
  using type = std::int64_t;
};

template <std::size_t SizeOfInt>
  requires IsSupportedSizeOfInt<SizeOfInt>
using signed_int_t = typename signed_int<SizeOfInt>::type;

template <std::size_t SizeOfPtr>
  requires IsSupportedSizeOfPtr<SizeOfPtr>
intptr_t read_optional_ptr(
  const MemoryRead& memory_read, intptr_t cursor, intptr_t& target
) {
  signed_int_t<SizeOfPtr> ptr{};
  cursor = memory_read(cursor, sizeof(ptr), &ptr);
  target = static_cast<intptr_t>(ptr);
  return cursor;
}

template <std::size_t SizeOfPtr>
  requires IsSupportedSizeOfPtr<SizeOfPtr>
intptr_t read_ptr(
  const MemoryRead& memory_read, intptr_t cursor, intptr_t& target
) {
  cursor = read_optional_ptr<SizeOfPtr>(memory_read, cursor, target);
  memory_read(target, 0, nullptr);  // check target is valid
  return cursor;
}

template <std::size_t N, std::size_t SizeOfPtr, typename T>
  requires IsSupportedSizeOfPtr<SizeOfPtr>
intptr_t read_layout_item(
  Pad<N>, const Memory<SizeOfPtr>& memory, intptr_t, intptr_t cursor, T&
) {
  return memory.read(cursor, N, nullptr);
}

template <std::size_t N, std::size_t SizeOfPtr, typename T>
  requires IsSupportedSizeOfPtr<SizeOfPtr>
intptr_t read_layout_item(
  Offset<N>, const Memory<SizeOfPtr>& memory, intptr_t base, intptr_t, T&
) {
  return memory.read(base, N, nullptr);
}

template <auto M, std::size_t SizeOfPtr, typename T>
  requires IsMemberTypeNativeLayout<M> && IsSupportedSizeOfPtr<SizeOfPtr>
intptr_t read_layout_item(
  Field<M>,
  const Memory<SizeOfPtr>& memory,
  intptr_t base,
  intptr_t cursor,
  T& target
) {
  using member_type = member_type_t<M>;
  auto& field = target.*M;
  return read(memory, cursor, field);
}

template <auto M, std::size_t SizeOfPtr, typename T>
  requires IsMemberTypeSame<M, intptr_t> && IsSupportedSizeOfPtr<SizeOfPtr>
intptr_t read_layout_item(
  FieldOptionalPtr<M>,
  const Memory<SizeOfPtr>& memory,
  intptr_t base,
  intptr_t cursor,
  T& target
) {
  using member_type = member_type_t<M>;
  auto& field = target.*M;
  return read_optional_ptr<SizeOfPtr>(memory.read, cursor, field);
}

template <auto M, std::size_t SizeOfPtr, typename T>
  requires IsMemberTypeSame<M, intptr_t> && IsSupportedSizeOfPtr<SizeOfPtr>
intptr_t read_layout_item(
  FieldPtr<M>,
  const Memory<SizeOfPtr>& memory,
  intptr_t base,
  intptr_t cursor,
  T& target
) {
  using member_type = member_type_t<M>;
  auto& field = target.*M;
  return read_ptr<SizeOfPtr>(memory.read, cursor, field);
}

template <IsLayoutItem... Items, std::size_t SizeOfPtr, typename T>
  requires IsSupportedSizeOfPtr<SizeOfPtr>
intptr_t read_layout(
  Layout<Items...>, const Memory<SizeOfPtr>& memory, intptr_t base, T& target
) {
  intptr_t cursor = base;
  // fold from first to last item
  ((cursor = read_layout_item(Items{}, memory, base, cursor, target)), ...);
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
template <std::size_t SizeOfPtr, HasRegisteredLayout T>
  requires IsSupportedSizeOfPtr<SizeOfPtr>
intptr_t read(const Memory<SizeOfPtr>& memory, intptr_t base, T& target) {
  return detail::read_layout(registered_layout_t<T>{}, memory, base, target);
}

}  // namespace mempeep

// ============================================================
// Example
// ============================================================

#include <iostream>

template <intptr_t N>
  requires mempeep::IsPositiveIntPtr<N>
struct MemoryReadMock {
  char data[N]{};

  intptr_t operator()(intptr_t cursor, intptr_t size, void* buffer) const {
    // handle overflow/underflow
    assert(size >= 0 && size <= N && cursor >= 0 && cursor <= N - size);
    if (buffer && size) std::memcpy(buffer, data + cursor, size);
    return cursor + size;
  }

  template <typename T>
  void write(intptr_t offset, T value) {
    assert(sizeof(value) <= N && offset >= 0 && offset <= N - sizeof(value));
    std::memcpy(data + offset, &value, sizeof(value));
  }
};

struct Pos {
  int x, y;
};

struct Player {
  int health;
  Pos pos;
  intptr_t target_ptr;
  intptr_t weapon_ptr;
};

template <>
struct mempeep::RegisterLayout<Pos> {
  using layout = Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>, Pad<4>>;
};

template <>
struct mempeep::RegisterLayout<Player> {
  using layout = Layout<
    Offset<8>,
    Field<&Player::health>,
    Offset<16>,
    Field<&Player::pos>,
    FieldOptionalPtr<&Player::target_ptr>,
    FieldPtr<&Player::weapon_ptr>>;
};

int main() {
  MemoryReadMock<64> memory_read{};
  memory_read.write(18, int32_t(123));
  memory_read.write(26, int32_t(11));
  memory_read.write(34, int32_t(22));
  memory_read.write(42, int16_t(-4));  // intentially invalid
  memory_read.write(44, int16_t(5));
  auto memory = mempeep::Memory<2>{memory_read};

  Player player{};
  mempeep::read(memory, 10, player);

  assert(player.health == 123);
  assert(player.pos.x == 11);
  assert(player.pos.y == 22);
  assert(player.target_ptr == -4);
  assert(player.weapon_ptr == 5);
}