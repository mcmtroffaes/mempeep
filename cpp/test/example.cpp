#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
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

using ReadRemote = std::function<bool(void*, intptr_t, std::size_t)>;

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

struct ReadError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

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
constexpr intptr_t apply_read_rule(
  Pad<N>, T&, const ReadRemote&, intptr_t, intptr_t cursor
) {
  return cursor + N;
}

template <typename T, std::size_t N>
constexpr intptr_t apply_read_rule(
  Offset<N>, T&, const ReadRemote&, intptr_t base, intptr_t
) {
  return base + N;
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
    if (!read_remote(&field, cursor, sizeof(field)))
      throw ReadError(
        "failed to read at remote address " + std::to_string(cursor)
      );
    return cursor + sizeof(field);
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

template <std::size_t N>
struct ReadRemoteMock {
  char data[N]{};

  bool operator()(void *buffer, intptr_t offset, std::size_t size) const {
    if (!buffer) return false;
    if (offset < 0) return false;
    auto uoffset = static_cast<std::size_t>(offset);
    if (uoffset > N || size > N - uoffset) return false;
    std::memcpy(buffer, data + uoffset, size);
    return true;
  }

  void write_int(std::size_t off, int v) {
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
  ReadRemoteMock<40> buf{};
  buf.write_int(18, 123);
  buf.write_int(26, 11);
  buf.write_int(34, 22);

  Player player{};
  read(player, buf, 10);

  assert(player.health == 123);
  assert(player.pos.x == 11);
  assert(player.pos.y == 22);
}