#pragma once

#include <cstdint>
#include <mempeep/layout.hpp>

namespace mempeep::test {

struct Pos {
  uint8_t x, y;

  // intentionally have padding bytes at end, for testing
  using remote_layout = Layout<Field<&Pos::x>, Field<&Pos::y>, Pad<2>>;
};

struct Player {
  uint8_t health;
  Pos pos;
  uint8_t target_ptr;
  uint16_t shop_ptr;  // wider than needed, must also work
  uint8_t weapon_ptr;
  Pos prev_pos;
  std::optional<Pos> tagged_pos;
  std::optional<Pos> house_pos;
  uint8_t mana;

  using remote_layout = Layout<
    Pad<2>,
    Field<&Player::health>,
    Pad<1>,
    Field<&Player::pos>,
    RawAddr<&Player::target_ptr>,
    RawAddr<&Player::shop_ptr>,
    RawAddr<&Player::weapon_ptr>,
    Ref<&Player::prev_pos>,
    NullableRef<&Player::tagged_pos>,
    NullableRef<&Player::house_pos>,
    Field<&Player::mana>>;
};

struct Game {
  uint8_t level;
  Player player;

  using remote_layout
    = Layout<Seek<1>, Field<&Game::level>, Seek<4>, Field<&Game::player>>;
};

}  // namespace mempeep::test