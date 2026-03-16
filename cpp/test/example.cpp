#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>

// ============================================================
// Public API
// ============================================================

template <auto Member>
struct Field {};

template <std::size_t N>
struct Pad {};

template <std::size_t N>
struct Offset {};

template <typename... Items>
struct Layout {};

template <typename T>
struct Remote;  // user specialization

// Reads memory of given size into buffer (if not null),
// and returns cursor advanced by size.
// Caller is responsible for pointer validation, handling overflows, etc.
using ReadRemote = std::function<intptr_t(void*, intptr_t, intptr_t)>;

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

// ============================================================
// read
// ============================================================

// forward declaration for recursive read in Field
template <typename T>
intptr_t read(T& out, const ReadRemote& read_remote, intptr_t base);

// if Item is not specialized then we must have an invalid item in Layout
template <typename T, typename Item>
intptr_t apply_read_rule(Item, T&, const ReadRemote&, intptr_t, intptr_t) {
  static_assert(!std::is_same_v<Item, Item>, "Unsupported layout item");
}

template <typename T, std::size_t N>
intptr_t apply_read_rule(
  Pad<N>, T&, const ReadRemote& read_remote, intptr_t, intptr_t cursor
) {
  return read_remote(nullptr, cursor, N);
}

template <typename T, std::size_t N>
constexpr intptr_t apply_read_rule(
  Offset<N>, T&, const ReadRemote& read_remote, intptr_t base, intptr_t
) {
  return read_remote(nullptr, base, N);
}

template <typename T, auto Member>
intptr_t apply_read_rule(
  Field<Member>,
  T& out,
  const ReadRemote& read_remote,
  intptr_t base,
  intptr_t cursor
) {
  using member_type = typename member_pointer_traits<Member>::member_type;
  auto& field = out.*Member;
  if constexpr (has_remote_v<member_type>) {
    // read returns the new cursor after the nested struct
    return read(field, read_remote, cursor);
  } else {
    return read_remote(&field, cursor, sizeof(field));
  }
}

template <typename T, typename... Items>
intptr_t read_layout(
  Layout<Items...>, T& out, const ReadRemote& read_remote, intptr_t base
) {
  intptr_t cursor = base;
  // fold from first to last item
  ((cursor = apply_read_rule(Items{}, out, read_remote, base, cursor)), ...);
  return cursor;
}

template <typename T>
intptr_t read(T& out, const ReadRemote& read_remote, intptr_t base) {
  static_assert(has_remote_v<T>, "Remote<T> must be specialized");
  return read_layout(remote_layout_t<T>{}, out, read_remote, base);
}

// ============================================================
// example
// ============================================================

#include <iostream>

template <intptr_t N>
struct ReadRemoteMock {
  char data[N]{};

  intptr_t operator()(void* buffer, intptr_t cursor, intptr_t size) const {
    // handle overflow/underflow first
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
struct Remote<Pos> {
  using layout = Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>, Pad<4>>;
};

template <>
struct Remote<Player> {
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
  read(player, buf, 10);

  assert(player.health == 123);
  assert(player.pos.x == 11);
  assert(player.pos.y == 22);
}