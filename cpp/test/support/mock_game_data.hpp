#pragma once

#include <doctest/doctest.h>

#include "mock_game.hpp"
#include "mock_memory.hpp"

namespace mempeep::test {

static constexpr char k_game_memory[]
  = "\x00\x00\x00\x00"      // 0:  unused
    "\x00\x01\x00\x00"      // 4:  pad(1), level = 1, pad(2)
    "\x00\x00\x7b\x00"      // 8:  pad(2), health = 123, pad(1)
    "\x0b\x16\x00\x00"      // 12: pos = (11, 22, pad(2))
    "\x00"                  // 16: target_ptr = 0
    "\x02"                  // 17: shop_ptr = 2
    "\x06"                  // 18: weapon_ptr = 6
    "\x1a"                  // 19: prev_pos ref = 26
    "\x1e"                  // 20: tagged_pos ref = 30
    "\x00"                  // 21: house_pos ref = 0 (null)
    "\x2f"                  // 22: mana = 47
    "\x00\x00\x00"          // 23: unused
    "\x58\x63\x00\x00"      // 26: prev_pos = (88, 99, pad(2))
    "\x37\x42\x00\x00";     // 30: tagged_pos = (55, 66, pad(2))

using GameReader = MockMemoryReader<uint8_t>;

inline GameReader make_game_reader() { return GameReader{k_game_memory}; }

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
  SUBCASE("tagged_pos") {
    REQUIRE(game.player.tagged_pos.has_value());
    CHECK(game.player.tagged_pos->x == 55);
    CHECK(game.player.tagged_pos->y == 66);
  }
  CHECK(!game.player.house_pos.has_value());
}

}  // namespace mempeep::test