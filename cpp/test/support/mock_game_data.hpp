#pragma once

#include <doctest/doctest.h>

#include "mock_game.hpp"
#include "mock_memory.hpp"

namespace mempeep::test {

using GameReader = MockMemoryReader<uint16_t, 1, 128>;

// reader for game data (base 4)
inline GameReader make_game_reader() {
  GameReader reader{};
  REQUIRE(reader.write(18, int32_t{123}));  // health
  REQUIRE(reader.write(26, int32_t{11}));  // pos.x
  REQUIRE(reader.write(34, int32_t{22}));   // pos.y
  REQUIRE(reader.write(42, uint16_t{0}));  // target_ptr
  REQUIRE(reader.write(44, uint16_t{2}));   // shop_ptr; u16 remote, u32 native
  REQUIRE(reader.write(46, uint16_t{6}));  // weapon_ptr
  REQUIRE(reader.write(48, uint16_t{60}));  // prev_pos ref
  REQUIRE(reader.write(50, uint16_t{80}));  // tagged_pos nullable ref
  REQUIRE(reader.write(52, uint16_t{0}));   // house_pos nullable ref
  REQUIRE(reader.write(54, int32_t{47}));   // mana
  REQUIRE(reader.write(60, int32_t{88}));   // prev_pos.x
  REQUIRE(reader.write(68, int32_t{99}));   // prev_pos.y
  REQUIRE(reader.write(80, int32_t{55}));   // tagged_pos.x
  REQUIRE(reader.write(88, int32_t{66}));   // tagged_pos.y
  return reader;
}

inline void check_game(const Game& game) {
  CHECK(game.player.health == 123);
  CHECK(game.player.pos.x == 11);
  CHECK(game.player.pos.y == 22);
  CHECK(game.player.target_ptr == 0);
  CHECK(game.player.shop_ptr == 2);
  CHECK(game.player.weapon_ptr == 6);
  CHECK(game.player.prev_pos.x == 88);
  CHECK(game.player.prev_pos.y == 99);
  CHECK(game.player.mana == 47);
  CHECK(game.player.tagged_pos.has_value());
  CHECK(game.player.tagged_pos->x == 55);
  CHECK(game.player.tagged_pos->y == 66);
  CHECK(!game.player.house_pos.has_value());
}

}  // namespace mempeep::test