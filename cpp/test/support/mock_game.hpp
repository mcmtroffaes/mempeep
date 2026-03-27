#pragma once

#include <cstdint>
#include <mempeep/descriptor.hpp>

namespace mempeep::test {

using TUInt8 = Primitive<uint8_t>;

struct Pos {
  uint8_t x, y;
};

// intentionally have padding bytes at end, for testing
using TPos
  = Struct<Pos, Fields<Field<TUInt8, &Pos::x>, Field<TUInt8, &Pos::y>, Pad<2>>>;

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
};

using TPlayer = Struct<
  Player,
  Fields<
    Pad<2>,
    Field<TUInt8, &Player::health>,
    Pad<1>,
    Field<TPos, &Player::pos>,
    Field<RawAddr<uint8_t>, &Player::target_ptr>,
    Field<RawAddr<uint16_t>, &Player::shop_ptr>,
    Field<RawAddr<uint8_t>, &Player::weapon_ptr>,
    Field<Ref<TPos>, &Player::prev_pos>,
    Field<NullableRef<TPos>, &Player::tagged_pos>,
    Field<NullableRef<TPos>, &Player::house_pos>,
    Field<TUInt8, &Player::mana>,
    Pad<1>>>;

struct Game {
  uint8_t level;
  std::string message;
  Player player;
  std::array<Pos, 2> hands;
  std::vector<Pos> pets;
  std::vector<Cave> caves;
};

using TGame = Struct<
  Game,
  Fields<
    Seek<1>,
    Field<TUInt8, &Game::level>,
    Field<Ref<LenString<uint8_t, 0x40>>, &Game::message>,
    Seek<4>,
    Field<TPlayer, &Game::player>,
    Field<Array<TPos, 2>, &Game::hands>,
    Field<Vector<TPos, 0x1000>, &Game::pets>,
    Field<CircularList<TCave, &Cave::next, 0x1000>, &Game::caves>>>;

}  // namespace mempeep::test