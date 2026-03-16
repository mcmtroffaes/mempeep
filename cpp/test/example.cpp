#include <cstddef>
#include <type_traits>

template <typename T> struct Remote;

// member type extraction
template <typename M, typename C> consteval M member_type_impl(M C::*);
template <auto Member> using member_type_t = decltype(member_type_impl(Member));

// detect whether Remote<T> exists and has advance(0)
template <typename T, typename = void> struct has_remote : std::false_type {};
template <typename T>
struct has_remote<T, std::void_t<decltype(Remote<T>::advance(std::size_t{}))>>
    : std::true_type {};
template <typename T> inline constexpr bool has_remote_v = has_remote<T>::value;

// remote size of any type
// use Remote<T>::advance(0) if it exists, otherwise fall back on sizeof(T)
template <typename T> consteval std::size_t remote_size() {
  if constexpr (has_remote_v<T>) {
    return Remote<T>::advance(0);
  } else {
    return sizeof(T);
  }
}

// layout field
// advances cursor by adding the size of a member pointer
template <auto Member> struct Field {
  using type = member_type_t<Member>;

  static consteval std::size_t advance(std::size_t cursor) {
    return cursor + remote_size<type>();
  }
};

// layout offset
// advances cursor by setting it to a fixed value
template <std::size_t N> struct Offset {
  static consteval std::size_t advance(std::size_t) { return N; }
};

// layout pad
// advances cursor by adding a constant
template <std::size_t N> struct Pad {
  static consteval std::size_t advance(std::size_t cursor) {
    return cursor + N;
  }
};

// remote layout
// Es packs any layout descriptors, i.e. anything that has Es::advance(cursor)
template <typename... Es> struct Layout {
  static consteval std::size_t advance(std::size_t cursor) {
    ((cursor = Es::advance(cursor)), ...);
    return cursor;
  }
};

// example

struct Pos {
  int x;
  int y;
};

struct Player {
  int health;
  Pos pos;
};

template <>
struct Remote<Pos> : Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>, Pad<4>> {};

template <>
struct Remote<Player> : Layout<Offset<12>, Field<&Player::health>, Offset<32>,
                               Field<&Player::pos>> {};

static_assert(remote_size<Pos>() == 16);
static_assert(remote_size<Player>() == 48);

int main() { return 0; };
