#pragma once

#include <doctest/doctest.h>

#include <string_view>

#include "mock_game.hpp"
#include "mock_memory.hpp"

namespace mempeep::test {

static constexpr char game_data[]
  = "\x00\x00\x00\x00"  // 0:  unused
    "\x00\x11\x00\x00"  // 4:  pad(1), level = 17, pad(2)
    "\x00\x00\x7b\x00"  // 8:  pad(2), health = 123, pad(1)
    "\x0b\x16\x00\x00"  // 12: pos = (11, 22, pad(2))
    "\x00"              // 16: target_ptr = 0
    "\x02"              // 17: shop_ptr = 2
    "\x06"              // 18: weapon_ptr = 6
    "\x24"              // 19: prev_pos ref = 36
    "\x28"              // 20: tagged_pos nullable ref = 40
    "\x00"              // 21: house_pos nullable ref = 0 (null)
    "\x2f"              // 22: mana = 47
    "\x00"              // 23: pad(1)
    "\x01\x02\x00\x00"  // 24: hands[0] = (1, 2, pad(2))
    "\x03\x04\x00\x00"  // 28: hands[1] = (1, 2, pad(2))
    "\x2c\x38"          // 32: pets vec (44, 48)
    "\x3c\x00"          // 34: caves circular list = 60, unused(1)
    "\x58\x63\x00\x00"  // 36: prev_pos = (88, 99, pad(2))
    "\x37\x42\x00\x00"  // 40: tagged_pos = (55, 66, pad(2))
    "\x05\x06\x00\x00"  // 44: pets[0] = (5, 6, pad(2))
    "\x07\x08\x00\x00"  // 48: pets[1] = (7, 8, pad(2))
    "\x09\x0a\x00\x00"  // 52: pets[2] = (9, 10, pad(2))
    "\x00\x00\x00\x00"  // 56: unused
    "\x10\x3e"          // 60: caves[0] = (16, 62)
    "\x12\x40"          // 62: caves[1] = (18, 64)
    "\x14\x42"          // 64: caves[2] = (20, 66)
    "\x16\x3c";         // 66: caves[3] = (22, 60)

inline void check_game(const Game& game) {
  SUBCASE("level") { CHECK_EQ(game.level, 17); }
  SUBCASE("player") {
    CHECK_EQ(game.player.health, 123);
    CHECK_EQ(game.player.pos.x, 11);
    CHECK_EQ(game.player.pos.y, 22);
    CHECK_EQ(game.player.target_ptr, 0);
    CHECK_EQ(game.player.shop_ptr, 2);
    CHECK_EQ(game.player.weapon_ptr, 6);
    CHECK_EQ(game.player.prev_pos.x, 88);
    CHECK_EQ(game.player.prev_pos.y, 99);
    CHECK_EQ(game.player.mana, 47);
    SUBCASE("tagged_pos") {
      REQUIRE(game.player.tagged_pos.has_value());
      CHECK_EQ(game.player.tagged_pos->x, 55);
      CHECK_EQ(game.player.tagged_pos->y, 66);
    }
    CHECK(!game.player.house_pos.has_value());
  }
  SUBCASE("hands") {
    CHECK_EQ(game.hands[0].x, 1);
    CHECK_EQ(game.hands[0].y, 2);
    CHECK_EQ(game.hands[1].x, 3);
    CHECK_EQ(game.hands[1].y, 4);
  }
  SUBCASE("pets") {
    REQUIRE_EQ(game.pets.size(), 3);
    CHECK_EQ(game.pets[0].x, 5);
    CHECK_EQ(game.pets[0].y, 6);
    CHECK_EQ(game.pets[1].x, 7);
    CHECK_EQ(game.pets[1].y, 8);
    CHECK_EQ(game.pets[2].x, 9);
    CHECK_EQ(game.pets[2].y, 10);
  }
}

}  // namespace mempeep::test