#include <cassert>      // assert
#include <cstdint>      // std::intptr_t, ...
#include <cstring>      // std::memcpy
#include <expected>     // std::expected
#include <functional>   // std::function
#include <type_traits>  // std::is_same_v, ...

namespace mempeep {

// ============================================================
// Public API: Layout, LayoutOf, ReadMemory
// ============================================================

template <auto MemberPtr>
struct Field {};

template <std::intptr_t N>
struct Pad {
  static_assert(N > 0);
};

template <std::intptr_t N>
struct Offset {
  static_assert(N > 0);
};

// Never null pointer, stores the pointee.
template <auto MemberPtr>
struct FieldRef {};  // TODO implement

// Possible null pointer, stores the pointee via std::optional<T>.
template <auto MemberPtr>
struct FieldOptionalRef {};  // TODO implement

// Never null pointer, stores remote pointer (intptr_t).
template <auto MemberPtr>
struct FieldPtr {};

// Possible null pointer, stores remote pointer (intptr_t).
template <auto MemberPtr>
struct FieldOptionalPtr {};

/**
 * @brief Defines a remote layout.
 *
 * Each item must be a Field, Pad, or Offset.
 * Example:
 *
 *   Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>>
 */
template <typename... Items>
struct Layout {};

/**
 * @brief Template for registering the remote layout of a native struct.
 *
 * To be specialized by the user for each struct with non-native remote layout.
 * Example:
 *
 *   template <>
 *   struct LayoutOf<Pos> {
 *     using layout = Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>>;
 *   };
 *
 * Valid items in a layout are Field<&Class::member>, Pad<N>, and Offset<N>.
 */
template <typename T>
struct LayoutOf;

/**
 * @brief Copy remote memory to a native buffer and advance remote pointer.
 *
 * To be implemented by the user.
 * Reads memory of given size into buffer (if valid),
 * and returns cursor advanced by size (or invalid cursor if read failed).
 * If size is also null, simply checks that cursor is valid.
 * The implementation is responsible for error handling (pointer validation,
 * handling overflows, etc.). This allows the user to decide if they want to
 * keep try reading remote layouts even after some errors.
 *
 * If you want to read as much as possible:
 * - Pick an invalid pointer value as a sentinel to indicate error (i.e. 0).
 * - Return the sentinel if the read fails for any reason.
 * - If given the sentinel as source pointer, return it immediately (skip read).
 *
 * If you want to fail as quickly as possible, raise an exception.
 *
 * @param cursor Remote source pointer.
 * @param size   Number of bytes to copy.
 * @param buffer Native destination buffer.
 * @return       Updated remote pointer.
 */
using MemoryRead = std::function<intptr_t(intptr_t, intptr_t, void*)>;

/**
 * @brief Remote memory abstraction layer.
 *
 * Encapsulates reading as well as the size of remote pointers.
 * The read function handles all pointer arithmetic and validation.
 */
template <std::size_t SizeOfPtr>
struct Memory {
  MemoryRead read = [](intptr_t cursor, intptr_t size, void* buffer) {
    // simplest implementation: no reading, no error checking
    return cursor + size;
  };
};

/**
 * @brief Check LayoutOf<T>::layout exists.
 */
template <typename T, typename... Items>
concept HasLayout = requires { typename LayoutOf<T>::layout; };

/**
 * @brief Shorthand for LayoutOf<T>::layout.
 */
template <typename T>
using layout_t = typename LayoutOf<T>::layout;

// Forward declaration to support recursive Layout reading.
template <std::size_t SizeOfPtr, typename T>
  requires HasLayout<T>
intptr_t read(const Memory<SizeOfPtr>& memory, intptr_t base, T& target);

namespace detail {

// ============================================================
// Helpers
// ============================================================

template <auto>
struct member_pointer_traits;

template <typename C, typename T, T C::* MemberPtr>
struct member_pointer_traits<MemberPtr> {
  using class_type = C;
  using member_type = T;
};

template <std::size_t Size>
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

template <std::size_t Size>
using signed_int_t = typename signed_int<Size>::type;

template <std::size_t SizeOfPtr>
intptr_t read_optional_ptr(
  const MemoryRead& memory_read, intptr_t cursor, intptr_t& target
) {
  signed_int_t<SizeOfPtr> ptr{};
  static_assert(sizeof(ptr) == SizeOfPtr);
  static_assert(sizeof(intptr_t) >= SizeOfPtr);
  cursor = memory_read(cursor, sizeof(ptr), &ptr);
  target = static_cast<intptr_t>(ptr);
  return cursor;
}

template <std::size_t SizeOfPtr>
intptr_t read_ptr(
  const MemoryRead& memory_read, intptr_t cursor, intptr_t& target
) {
  cursor = read_optional_ptr<SizeOfPtr>(memory_read, cursor, target);
  memory_read(target, 0, nullptr);  // check target is valid
  return cursor;
}

template <typename Item, std::size_t SizeOfPtr, typename T>
intptr_t read_layout_item(
  Item, const Memory<SizeOfPtr>&, intptr_t, intptr_t, T&
) {
  static_assert(!std::is_same_v<Item, Item>, "Unsupported layout item");
}

template <std::size_t N, std::size_t SizeOfPtr, typename T>
intptr_t read_layout_item(
  Pad<N>, const Memory<SizeOfPtr>& memory, intptr_t, intptr_t cursor, T&
) {
  return memory.read(cursor, N, nullptr);
}

template <std::size_t N, std::size_t SizeOfPtr, typename T>
intptr_t read_layout_item(
  Offset<N>, const Memory<SizeOfPtr>& memory, intptr_t base, intptr_t, T&
) {
  return memory.read(base, N, nullptr);
}

template <auto MemberPtr, std::size_t SizeOfPtr, typename T>
intptr_t read_layout_item(
  Field<MemberPtr>,
  const Memory<SizeOfPtr>& memory,
  intptr_t base,
  intptr_t cursor,
  T& target
) {
  using member_type = typename member_pointer_traits<MemberPtr>::member_type;
  auto& field = target.*MemberPtr;
  if constexpr (HasLayout<member_type>) {
    return read(memory, cursor, field);
  } else {
    return memory.read(cursor, sizeof(field), &field);
  }
}

template <auto MemberPtr, std::size_t SizeOfPtr, typename T>
intptr_t read_layout_item(
  FieldOptionalPtr<MemberPtr>,
  const Memory<SizeOfPtr>& memory,
  intptr_t base,
  intptr_t cursor,
  T& target
) {
  using member_type = typename member_pointer_traits<MemberPtr>::member_type;
  static_assert(
    std::is_same_v<member_type, intptr_t>,
    "FieldOptionalPtr needs member type to be intptr_t"
  );
  auto& field = target.*MemberPtr;
  return read_optional_ptr<SizeOfPtr>(memory.read, cursor, field);
}

template <auto MemberPtr, std::size_t SizeOfPtr, typename T>
intptr_t read_layout_item(
  FieldPtr<MemberPtr>,
  const Memory<SizeOfPtr>& memory,
  intptr_t base,
  intptr_t cursor,
  T& target
) {
  using member_type = typename member_pointer_traits<MemberPtr>::member_type;
  static_assert(
    std::is_same_v<member_type, intptr_t>,
    "FieldPtr needs member type to be intptr_t"
  );
  auto& field = target.*MemberPtr;
  return read_ptr<SizeOfPtr>(memory.read, cursor, field);
}

template <typename... Items, std::size_t SizeOfPtr, typename T>
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
 * @brief Read native type T from remote memory using LayoutOf<T>::layout.
 *
 * Note that the implementation only uses memory.read for advancing the
 * pointer. In particular, both Pad and Offset are implemented by a call to
 * memory.read.
 *
 * Consequently, all error handling concerning pointer overflows or invalid
 * pointers is delegated to memory.read.
 *
 * @param memory  Manages remote memory.
 * @param base    Remote base pointer of the struct.
 * @param target  Native destination.
 * @return        The outcome of the final read call.
 */
template <std::size_t SizeOfPtr, typename T>
  requires HasLayout<T>
intptr_t read(const Memory<SizeOfPtr>& memory, intptr_t base, T& target) {
  return detail::read_layout(layout_t<T>{}, memory, base, target);
}

}  // namespace mempeep

// ============================================================
// Example
// ============================================================

#include <iostream>

template <intptr_t N>
struct MemoryReadMock {
  static_assert(N > 0);
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
struct mempeep::LayoutOf<Pos> {
  using layout = Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>, Pad<4>>;
};

template <>
struct mempeep::LayoutOf<Player> {
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