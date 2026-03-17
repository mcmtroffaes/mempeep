#include <cassert>      // assert
#include <cstdint>      // std::intptr_t
#include <cstring>      // std::memcpy
#include <functional>   // std::function
#include <type_traits>  // std::void_t, ...

namespace mempeep {

// ============================================================
// Public API: Layout, StructLayout, ReadMemory
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
 *   struct StructLayout<Pos> {
 *     using layout = Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>>;
 *   };
 *
 * Valid items in a layout are Field<&Class::member>, Pad<N>, and Offset<N>.
 */
template <typename T>
struct StructLayout;

/**
 * @brief Copy remote memory to a native buffer and advance remote pointer.
 *
 * To be implemented by the user.
 * Reads memory of given size into buffer (if not null),
 * and returns cursor advanced by size.
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
using ReadMemory = std::function<intptr_t(intptr_t, intptr_t, void*)>;

/**
 * @brief Check StuctLayout<T>::layout exists.
 */
template <typename T, typename... Items>
concept HasLayout = requires { typename StructLayout<T>::layout; };

/**
 * @brief Shorthand for StuctLayout<T>::layout.
 */
template <typename T>
using layout_t = typename StructLayout<T>::layout;

// Forward declaration to support recursive Layout reading.
template <typename T>
  requires HasLayout<T>
intptr_t read(const ReadMemory& read_memory, intptr_t base, T& target);

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

// If Item is not specialized then we must have an invalid item in Layout.
template <typename T, typename Item>
intptr_t read_layout_item(Item, const ReadMemory&, intptr_t, intptr_t, T&) {
  static_assert(!std::is_same_v<Item, Item>, "Unsupported layout item");
}

// Specialization for Pad<N>.
template <typename T, std::size_t N>
intptr_t read_layout_item(
  Pad<N>, const ReadMemory& read_memory, intptr_t, intptr_t cursor, T&
) {
  return read_memory(cursor, N, nullptr);
}

// Specialization for Offset<N>.
template <typename T, std::size_t N>
intptr_t read_layout_item(
  Offset<N>, const ReadMemory& read_memory, intptr_t base, intptr_t, T&
) {
  return read_memory(base, N, nullptr);
}

// Specialization for Field<Member>.
template <typename T, auto MemberPtr>
intptr_t read_layout_item(
  Field<MemberPtr>,
  const ReadMemory& read_memory,
  intptr_t base,
  intptr_t cursor,
  T& target
) {
  using member_type = typename member_pointer_traits<MemberPtr>::member_type;
  auto& field = target.*MemberPtr;
  if constexpr (HasLayout<member_type>) {
    return read(read_memory, cursor, field);
  } else {
    return read_memory(cursor, sizeof(field), &field);
  }
}

template <typename T, typename... Items>
intptr_t read_layout(
  Layout<Items...>, const ReadMemory& read_memory, intptr_t base, T& target
) {
  intptr_t cursor = base;
  // fold from first to last item
  ((cursor = read_layout_item(Items{}, read_memory, base, cursor, target)),
   ...);
  return cursor;
}

}  // namespace detail

// ============================================================
// Public API: read
// ============================================================

/**
 * @brief Read native type T from remote memory using StructLayout<T>::layout.
 *
 * Note that the implementation only uses read_memory for advancing the
 * pointer. In particular, both Pad and Offset are implemented by a call to
 * read_memory.
 *
 * Consequently, all error handling concerning pointer overflows or invalid
 * pointers is delegated to read_memory.
 *
 * @param read_memory Function for reading bytes from remote memory.
 * @param base               Remote base pointer of the struct.
 * @param target             Native destination.
 * @return                   The outcome of the final read_memory call.
 */
template <typename T>
  requires HasLayout<T>
intptr_t read(const ReadMemory& read_memory, intptr_t base, T& target) {
  return detail::read_layout(layout_t<T>{}, read_memory, base, target);
}

}  // namespace mempeep

// ============================================================
// Example
// ============================================================

#include <iostream>

template <intptr_t N>
struct ReadMemoryMock {
  char data[N]{};

  intptr_t operator()(intptr_t cursor, intptr_t size, void* buffer) const {
    // handle overflow/underflow
    assert(size >= 0 && size <= N && cursor >= 0 && cursor <= N - size);
    if (buffer && size) std::memcpy(buffer, data + cursor, size);
    return cursor + size;
  }

  void write_int(intptr_t offset, int value) {
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
};

template <>
struct mempeep::StructLayout<Pos> {
  using layout = Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>, Pad<4>>;
};

template <>
struct mempeep::StructLayout<Player> {
  using layout = Layout<
    Offset<8>,
    Field<&Player::health>,
    Offset<16>,
    Field<&Player::pos>>;
};

int main() {
  ReadMemoryMock<48> buf{};
  buf.write_int(18, 123);
  buf.write_int(26, 11);
  buf.write_int(34, 22);

  Player player{};
  mempeep::read(buf, 10, player);

  assert(player.health == 123);
  assert(player.pos.x == 11);
  assert(player.pos.y == 22);
}