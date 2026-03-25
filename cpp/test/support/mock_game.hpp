#pragma once

#include <cstdint>
#include <mempeep/layout.hpp>

namespace mempeep::test {

using TUInt8 = Primitive<uint8_t>;

struct Pos {
  uint8_t x, y;

  // intentionally have padding bytes at end, for testing
  using fields = Fields<Field<TUInt8, &Pos::x>, Field<TUInt8, &Pos::y>, Pad<2>>;
};

using TPos = Struct<Pos>;

struct Cave {
  uint8_t id;
  uint8_t next;
};

using TCave = Primitive<Cave>;

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

  using fields = Fields<
    Pad<2>,
    Field<TUInt8, &Player::health>,
    Pad<1>,
    Field<TPos, &Player::pos>,
    Field<RawAddr<uint8_t>, & Player::target_ptr>,
        Field<
          RawAddr<uint16_t>, &Player::shop_ptr>,
    Field<RawAddr<uint8_t>, &Player::weapon_ptr>,
    Field<Ref<TPos>, &Player::prev_pos>,
    Field<NullableRef<TPos>, &Player::tagged_pos>,
    Field<NullableRef<TPos>, &Player::house_pos>,
    Field<TUInt8, &Player::mana>,
    Pad<1>>;
};

using TPlayer = Struct<Player>;

struct Game {
  uint8_t level;
  Player player;
  std::array<Pos, 2> hands;
  std::vector<Pos> pets;
  std::vector<Cave> caves;

  using fields = Fields<
    Seek<1>,
    Field<TUInt8, &Game::level>,
    Seek<4>,
    Field<TPlayer, &Game::player>,
    Field<Array<TPos, 2>, &Game::hands>,
    Field<Vector<TPos, 0x1000>, &Game::pets>>;
    // CircularList<&Game::caves, &Cave::next, 100>>;
};

using TGame = Struct<Game>;

}  // namespace mempeep::test