#include <cassert>      // assert
#include <cstdint>      // std::intptr_t
#include <cstring>      // std::memcpy
#include <functional>   // std::function
#include <type_traits>  // std::void_t, ...

namespace mempeep {

// ============================================================
// Public API: Layout & ReadRemote
// ============================================================

template <auto Member>
struct Field {};

template <std::size_t N>
struct Pad {};

template <std::size_t N>
struct Offset {};

template <typename... Items>
struct Layout {};

/**
 * @brief Template for registering the remote layout of a native struct.
 *
 * To be specialized by the user for each struct with non-native remote layout.
 * Example:
 *
 *   template <>
 *   struct Remote<Pos> {
 *     using layout = Layout<Field<&Pos::x>, Field<&Pos::y>, Field<&Pos::z>>;
 *   };
};
 */
template <typename T>
struct Remote;

/**
 * @brief Copy remote memory to a native buffer and advance remote pointer.
 *
 * To be implemented by the user.
 * Reads memory of given size into buffer (if not null),
 * and returns cursor advanced by size.
 * Responsible for pointer validation, handling overflows, etc.
 *
 * @param cursor Remote source.
 * @param size Number of bytes to copy.
 * @param buffer Native destination.
 * @return An integer result after processing.
 */
using ReadRemote = std::function<intptr_t(intptr_t, intptr_t, void*)>;

// Forward declaration to support recursive Layout reading.
template <typename T>
intptr_t read(const ReadRemote& read_remote, intptr_t base, T& out);

namespace detail {

// ============================================================
// Helpers
// ============================================================

template <auto>
struct member_pointer_traits;

template <typename Class, typename Member, Member Class::* Ptr>
struct member_pointer_traits<Ptr> {
  using class_type = Class;
  using member_type = Member;
};

template <typename T>
using remote_layout_t = typename Remote<T>::layout;

template <typename T, typename = void>
inline constexpr bool has_remote_v = false;

template <typename T>
inline constexpr bool has_remote_v<T, std::void_t<remote_layout_t<T>>> = true;

// If Item is not specialized then we must have an invalid item in Layout.
template <typename T, typename Item>
intptr_t apply_read_rule(Item, const ReadRemote&, intptr_t, intptr_t, T&) {
  static_assert(!std::is_same_v<Item, Item>, "Unsupported layout item");
}

// Specialization for Pad<N>.
template <typename T, std::size_t N>
intptr_t apply_read_rule(
  Pad<N>, const ReadRemote& read_remote, intptr_t, intptr_t cursor, T&
) {
  return read_remote(cursor, N, nullptr);
}

// Specialization for Offset<N>.
template <typename T, std::size_t N>
constexpr intptr_t apply_read_rule(
  Offset<N>, const ReadRemote& read_remote, intptr_t base, intptr_t, T&
) {
  return read_remote(base, N, nullptr);
}

// Specialization for Field<Member>.
template <typename T, auto Member>
intptr_t apply_read_rule(
  Field<Member>,
  const ReadRemote& read_remote,
  intptr_t base,
  intptr_t cursor,
  T& out
) {
  using member_type = typename member_pointer_traits<Member>::member_type;
  auto& field = out.*Member;
  if constexpr (has_remote_v<member_type>) {
    // read returns the new cursor after the nested struct
    return read(read_remote, cursor, field);
  } else {
    return read_remote(cursor, sizeof(field), &field);
  }
}

template <typename T, typename... Items>
intptr_t read_layout(
  Layout<Items...>, const ReadRemote& read_remote, intptr_t base, T& out
) {
  intptr_t cursor = base;
  // fold from first to last item
  ((cursor = apply_read_rule(Items{}, read_remote, base, cursor, out)), ...);
  return cursor;
}

}  // namespace detail

// ============================================================
// Public API: read
// ============================================================

/**
 * @brief Read native type T from remote memory, using Remote<T>::layout if
 * needed.
 *
 * @param read_remote Function for reading bytes from remote memory.
 * @param base        Remote base pointer of the struct.
 * @param out         Native destination.
 * @return            Remote pointer to the memory right after.
 */
template <typename T>
intptr_t read(const ReadRemote& read_remote, intptr_t base, T& out) {
  static_assert(detail::has_remote_v<T>, "Remote<T> must be specialized");
  return detail::read_layout(
    detail::remote_layout_t<T>{}, read_remote, base, out
  );
}

}  // namespace mempeep

// ============================================================
// Example
// ============================================================

#include <iostream>

template <intptr_t N>
struct ReadRemoteMock {
  char data[N]{};

  intptr_t operator()(intptr_t cursor, intptr_t size, void* buffer) const {
    // handle overflow/underflow
    assert(
      size >= 0 && size < N && cursor >= 0 && cursor < N && size <= N - cursor
    );
    if (buffer) std::memcpy(buffer, data + cursor, size);
    return cursor + size;
  }

  void write_int(intptr_t off, int v) {
    assert(off + sizeof(v) <= N);
    std::memcpy(data + off, &v, sizeof(v));
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
struct mempeep::Remote<Pos> {
  using layout = Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>, Pad<4>>;
};

template <>
struct mempeep::Remote<Player> {
  using layout = Layout<
    Offset<8>,
    Field<&Player::health>,
    Offset<16>,
    Field<&Player::pos>>;
};

int main() {
  ReadRemoteMock<48> buf{};
  buf.write_int(18, 123);
  buf.write_int(26, 11);
  buf.write_int(34, 22);

  Player player{};
  mempeep::read(buf, 10, player);

  assert(player.health == 123);
  assert(player.pos.x == 11);
  assert(player.pos.y == 22);
}