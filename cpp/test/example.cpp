#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

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

using ReadRemote = std::function<bool(void *,      // buffer
                                      intptr_t,    // offset
                                      std::size_t  // size
                                      )>;

// ============================================================
// Helpers
// ============================================================

template <auto>
struct member_pointer_traits;

template <typename Class, typename Member, Member Class::*Ptr>
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

template <typename T>
struct always_false : std::false_type {};

struct ReadError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// ============================================================
// remote_size
// ============================================================

template <typename T>
constexpr std::size_t remote_size();

template <typename Item>
constexpr void apply_size_rule(Item, std::size_t &) {
  // can't just write static_assert(false, ...) because we only want
  // the compiler to evaluate this (and thus assert an error)
  // when the template is instantiated
  static_assert(always_false<Item>::value,
                "Unsupported layout item in Layout<>");
}

template <std::size_t N>
constexpr void apply_size_rule(Pad<N>, std::size_t &cursor) {
  cursor += N;
}

template <std::size_t N>
constexpr void apply_size_rule(Offset<N>, std::size_t &cursor) {
  cursor = N;
}

template <auto Member>
constexpr void apply_size_rule(Field<Member>, std::size_t &cursor) {
  using member_type = typename member_pointer_traits<Member>::member_type;

  if constexpr (has_remote_v<member_type>) {
    cursor += remote_size<member_type>();
  } else {
    cursor += sizeof(member_type);
  }
}

// calculate size of Layout<Items...>
template <typename... Items>
constexpr std::size_t remote_size(Layout<Items...>) {
  std::size_t cursor = 0;
  (apply_size_rule(Items{}, cursor), ...);
  return cursor;
}

// calculate size of Remote<T>::layout
template <typename T>
constexpr std::size_t remote_size() {
  return remote_size(remote_layout_t<T>{});
}

// ============================================================
// read
// ============================================================

template <typename T>
T read(const ReadRemote &read_remote, intptr_t base);

template <typename T, typename Item>
void apply_read_rule(T &out, const ReadRemote &read_remote, intptr_t base,
                     std::size_t &cursor) {
  static_assert(always_false<Item>::value, "Unsupported layout item");
}

template <typename T, std::size_t N>
void apply_read_rule(Pad<N>, T &, const ReadRemote &, intptr_t,
                     std::size_t &cursor) {
  cursor += N;
}

template <typename T, std::size_t N>
void apply_read_rule(Offset<N>, T &, const ReadRemote &, intptr_t,
                     std::size_t &cursor) {
  cursor = N;
}

template <typename T, auto Member>
void apply_read_rule(Field<Member>, T &out, const ReadRemote &read_remote,
                     intptr_t base, std::size_t &cursor) {
  using member_type = typename member_pointer_traits<Member>::member_type;

  auto remote_offset = base + static_cast<intptr_t>(cursor);
  auto &field = out.*Member;

  if constexpr (has_remote_v<member_type>) {
    field = read<member_type>(read_remote, remote_offset);
    cursor += remote_size<member_type>();
  } else {
    if (!read_remote(&field, remote_offset, sizeof(member_type))) {
      throw ReadError("failed to read field at remote offset " +
                      std::to_string(cursor));
    }
    cursor += sizeof(member_type);
  }
}

template <typename T, typename... Items>
T read_layout(Layout<Items...>, const ReadRemote &read_remote, intptr_t base) {
  T out{};
  std::size_t cursor = 0;
  (apply_read_rule(Items{}, out, read_remote, base, cursor), ...);
  return out;
}

template <typename T>
T read(const ReadRemote &read_remote, intptr_t base) {
  static_assert(has_remote_v<T>, "Remote<T> must be specialized");
  return read_layout<T>(remote_layout_t<T>{}, read_remote, base);
}

// ============================================================
// example
// ============================================================

template <std::size_t N>
struct ReadRemoteMock {
  char data[N];

  bool operator()(void *buffer, intptr_t offset, std::size_t size) const {
    if (!buffer) return false;
    if (offset < 0) return false;
    auto uoffset = static_cast<std::size_t>(offset);
    if (uoffset > N || size > N - uoffset) return false;
    std::memcpy(buffer, data + uoffset, size);
    return true;
  }

  auto write_int(std::size_t off, int v) {
    assert(off + sizeof(v) < N);
    std::memcpy(data + off, &v, sizeof(v));
  };
};

struct Pos {
  int x;
  int y;
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
  using layout = Layout<Offset<8>, Field<&Player::health>, Offset<16>,
                        Field<&Player::pos>>;
};

static_assert(remote_size<Pos>() == 16);
static_assert(remote_size<Player>() == 32);

int main() {
  // set up remote buffer
  ReadRemoteMock<40> buf{};
  buf.write_int(18, 123);
  buf.write_int(26, 11);
  buf.write_int(34, 22);
  // read and check
  Player player = read<Player>(buf, 10);
  assert(player.health == 123);
  assert(player.pos.x == 11);
  assert(player.pos.y == 22);
}