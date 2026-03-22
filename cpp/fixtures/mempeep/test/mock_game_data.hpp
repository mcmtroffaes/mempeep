#pragma once

#include <mempeep/test/mock_game.hpp>
#include <mempeep/test/mock_memory.hpp>

namespace mempeep::test {

using GameReader = MockMemoryReader<uint16_t, 1, 128>;

// reader for game data (base 4)
inline GameReader make_game_reader() {
  GameReader reader{};
  assert(reader.write(18, int32_t{123}));  // health
  assert(reader.write(26, int32_t{11}));   // pos.x
  assert(reader.write(34, int32_t{22}));   // pos.y
  assert(reader.write(42, uint16_t{0}));   // target_ptr
  assert(reader.write(44, uint16_t{2}));   // shop_ptr; u16 remote, u32 native
  assert(reader.write(46, uint16_t{6}));   // weapon_ptr
  assert(reader.write(48, uint16_t{60}));  // prev_pos ref
  assert(reader.write(50, uint16_t{80}));  // tagged_pos nullable ref
  assert(reader.write(52, uint16_t{0}));   // house_pos nullable ref
  assert(reader.write(54, int32_t{47}));   // mana
  assert(reader.write(60, int32_t{88}));   // prev_pos.x
  assert(reader.write(68, int32_t{99}));   // prev_pos.y
  assert(reader.write(80, int32_t{55}));   // tagged_pos.x
  assert(reader.write(88, int32_t{66}));   // tagged_pos.y
  return reader;
}

inline void assert_game(const Game& game) {
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

}  // namespace mempeep::test