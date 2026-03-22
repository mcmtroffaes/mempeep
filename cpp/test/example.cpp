#include <cassert>      // assert
#include <cstdint>      // std::int32_t, ...
#include <cstring>      // std::memcpy
#include <format>       // std::format
#include <iostream>     // std::cout, ...
#include <optional>     // std::optional
#include <type_traits>  // std::same_as, ...
#include <utility>      // std::ignore

#include "mempeep/read.hpp"
#include "mempeep/tracers.hpp"

// mock reader for testing
template <typename Address, auto BASE, auto N>
  requires(
    IsAddress<Address> && std::in_range<Address>(BASE)
    && std::in_range<std::size_t>(N)
  )
struct MockMemoryReader {
  using address_type = Address;
  static constexpr Address base = static_cast<Address>(BASE);
  static constexpr std::size_t n = static_cast<std::size_t>(N);
  std::byte data[n]{};

  bool operator()(Address address, std::size_t size, void* buffer) const {
    // check buffer exists and n is positive
    if (buffer == nullptr) return false;
    if (size == 0) return false;
    // handle overflow
    if (n < size) return false;        // ensure n - size >= 0
    if (address < base) return false;  // ensure address - base >= 0
    // both sides of inequality below are non-negative so safe to compare
    if (n - size < address - base) return false;  // ensure no overread
    // address - base <= n so now it is safe to cast to std::size_t
    std::memcpy(buffer, data + static_cast<std::size_t>(address - base), size);
    return true;
  }

  // note: this function is only safe in debug mode
  template <typename Addr, typename T>
  void write(Addr addr, T value) {
    assert(std::in_range<Address>(addr));
    const auto address = static_cast<Address>(addr);
    assert(n >= sizeof(T));   // ensure n - sizeof(T) >= 0
    assert(address >= base);  // ensure off - base >= 0
    // both sides of inequality below are non-negative so safe to compare
    assert(n - sizeof(T) >= address - base);  // ensure no overwrite
    // address - base <= n so now it is safe to cast to std::size_t
    std::memcpy(
      data + static_cast<std::size_t>(address - base), &value, sizeof(value)
    );
  }
};

struct Pos {
  int32_t x, y;
};

struct Player {
  int32_t health;
  Pos pos;
  uint16_t target_ptr;
  uint32_t shop_ptr;  // wider than needed, still fine
  uint16_t weapon_ptr;
  Pos prev_pos;
  std::optional<Pos> tagged_pos;
  std::optional<Pos> house_pos;
  int32_t mana;
};

struct Game {
  Player player;
};

using namespace mempeep;

static_assert(member_name<&Player::house_pos>() == "house_pos");

// intentionally have 4 padding bytes at end, for testing
auto remote_layout(remote_layout_tag<Pos>)
  -> Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>, Pad<4>>;

auto remote_layout(remote_layout_tag<Player>) -> Layout<
  Seek<8>,
  Field<&Player::health>,
  Seek<16>,
  Field<&Player::pos>,
  RawAddr<&Player::target_ptr>,
  RawAddr<&Player::shop_ptr>,
  RawAddr<&Player::weapon_ptr>,
  Ref<&Player::prev_pos>,
  NullableRef<&Player::tagged_pos>,
  NullableRef<&Player::house_pos>,
  Field<&Player::mana>>;

auto remote_layout(remote_layout_tag<Game>)
  -> Layout<Seek<6>, Field<&Game::player>>;

static void assert_game(const Game& game) {
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

int main() {
  MockMemoryReader<uint16_t, 1, 128> reader{};
  reader.write(18, int32_t{123});  // health
  reader.write(26, int32_t{11});   // pos.x
  reader.write(34, int32_t{22});   // pos.y
  reader.write(42, uint16_t{0});   // target_ptr
  reader.write(44, uint16_t{2});   // shop_ptr; u16 remote, u32 native
  reader.write(46, uint16_t{6});   // weapon_ptr
  reader.write(48, uint16_t{60});  // prev_pos ref
  reader.write(50, uint16_t{80});  // tagged_pos nullable ref
  reader.write(52, uint16_t{0});   // house_pos nullable ref
  reader.write(54, int32_t{47});   // mana
  reader.write(60, int32_t{88});   // prev_pos.x
  reader.write(68, int32_t{99});   // prev_pos.y
  reader.write(80, int32_t{55});   // tagged_pos.x
  reader.write(88, int32_t{66});   // tagged_pos.y
  uint16_t base{4};

  {
    Game game{};
    assert(mempeep::read_remote(reader, base, game));
  }
  {
    Game game{};
    assert(mempeep::read_remote(reader, base, game, LogTracer{}));
  }
}
