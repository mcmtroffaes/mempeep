#pragma once

#include <cstdint>
#include <mempeep/layout.hpp>

namespace mempeep::test {

struct Pos {
  int8_t x, y;
};

struct Player {
  int8_t health;
  Pos pos;
  uint8_t target_ptr;
  uint16_t shop_ptr;  // wider than needed, must also work
  uint8_t weapon_ptr;
  Pos prev_pos;
  std::optional<Pos> tagged_pos;
  std::optional<Pos> house_pos;
  int8_t mana;
};

struct Game {
  int8_t level;
  Player player;
};

// intentionally have padding bytes at end, for testing
auto remote_layout(remote_layout_tag<Pos>)
  -> Layout<Field<&Pos::x>, Field<&Pos::y>, Pad<2>>;

auto remote_layout(remote_layout_tag<Player>) -> Layout<
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

auto remote_layout(remote_layout_tag<Game>)
  -> Layout<Seek<1>, Field<&Game::level>, Seek<4>, Field<&Game::player>>;

}  // namespace mempeep::test