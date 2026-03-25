#pragma once

#include <cstdint>
#include <mempeep/layout.hpp>

namespace mempeep::test {

struct Pos {
  uint8_t x, y;

  // intentionally have padding bytes at end, for testing
  using fields = Layout<Field<&Pos::x>, Field<&Pos::y>, Pad<2>>;
};

struct Cave {
  uint8_t id;
  uint8_t next;

  using is_primitive_tag = void;
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

  using fields = Layout<
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
    Field<&Player::mana>,
    Pad<1>>;
};

struct Game {
  uint8_t level;
  Player player;
  std::array<Pos, 2> hands;
  std::vector<Pos> pets;
  std::vector<Cave> caves;

  using fields = Layout<
    Seek<1>,
    Field<&Game::level>,
    Seek<4>,
    Field<&Game::player>,
    Array<&Game::hands>,
    Vector<&Game::pets>,
    CircularList<&Game::caves, &Cave::next, 100>>;
};

}  // namespace mempeep::test