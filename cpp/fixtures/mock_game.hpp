#pragma once

#include <cstdint>
#include <mempeep/layout.hpp>

namespace mempeep::test {

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

}  // namespace mempeep::test