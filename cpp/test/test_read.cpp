#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <mempeep/read.hpp>
#include <optional>
#include <string_view>

#include "support/mock_game_data.hpp"

// truly empty data
// (can't use "" as it includes null terminator)
static constexpr std::array<uint8_t, 0> empty_data{};

TEST_CASE("successful read") {
  static constexpr uint8_t base{4};
  auto reader
    = mempeep::test::MockMemoryReader<uint8_t>{mempeep::test::game_data};
  mempeep::test::Game game{};
  mempeep::ErrorTracer tracer{};
  CHECK(mempeep::read(reader, base, game, tracer));
  mempeep::test::check_game(game);
}

TEST_CASE("failed read: complete failure") {
  auto reader = mempeep::test::MockMemoryReader<uint8_t>{empty_data};
  mempeep::test::Game game{};
  mempeep::ErrorTracer tracer{};
  CHECK(!mempeep::read(reader, 0, game, tracer));
}

TEST_CASE("failed read: invalid addresses") {
  static constexpr uint8_t base{4};
  static constexpr std::string_view data{mempeep::test::game_data, 23};
  auto reader = mempeep::test::MockMemoryReader<uint8_t>{data};
  mempeep::test::Game game{};
  mempeep::ErrorTracer tracer{};
  CHECK(!mempeep::read(reader, base, game, tracer));
  SUBCASE("level") { CHECK_EQ(game.level, 17); }
  SUBCASE("player") {
    CHECK_EQ(game.player.health, 123);
    CHECK_EQ(game.player.pos.x, 11);
    CHECK_EQ(game.player.pos.y, 22);
    CHECK_EQ(game.player.target_ptr, 0);
    CHECK_EQ(game.player.shop_ptr, 2);
    CHECK_EQ(game.player.weapon_ptr, 6);
    CHECK_EQ(game.player.prev_pos.x, 0);  // failed
    CHECK_EQ(game.player.prev_pos.y, 0);  // failed
    CHECK_EQ(game.player.mana, 47);
    SUBCASE("tagged_pos") {
      REQUIRE(game.player.tagged_pos.has_value());  // pointer was not null
      CHECK(game.player.tagged_pos->x == 0);        // failed
      CHECK(game.player.tagged_pos->y == 0);        // failed
    }
    CHECK(!game.player.house_pos.has_value());
  }
}

TEST_CASE("failed read: pad overflow") {
  struct Overflow {
    using fields
      = mempeep::Fields<mempeep::Pad<0xff>, mempeep::Pad<0xff>>;
  };

  auto reader = mempeep::test::MockMemoryReader<uint8_t>{empty_data};
  Overflow overflow{};
  mempeep::ErrorTracer tracer{};
  CHECK(!mempeep::read(reader, 0, overflow, tracer));
}

TEST_CASE("failed read: null ref") {
  struct Obj {
    uint8_t item;
    using fields = mempeep::Fields<mempeep::Ref<&Obj::item>>;
  };

  auto reader = mempeep::test::MockMemoryReader<uint8_t>{"\x00"};
  Obj obj{};
  mempeep::ErrorTracer tracer{};
  CHECK(!mempeep::read(reader, 0, obj, tracer));
}

TEST_CASE("failed read: missing ref") {
  struct Obj {
    uint8_t item;
    using fields = mempeep::Fields<mempeep::Ref<&Obj::item>>;
  };

  auto reader = mempeep::test::MockMemoryReader<uint8_t>{empty_data};
  Obj obj{};
  mempeep::ErrorTracer tracer{};
  CHECK(!mempeep::read(reader, 0, obj, tracer));
}

TEST_CASE("failed read: missing nullable ref") {
  struct Obj {
    std::optional<uint8_t> item;
    using fields = mempeep::Fields<mempeep::NullableRef<&Obj::item>>;
  };

  auto reader = mempeep::test::MockMemoryReader<uint8_t>{empty_data};
  Obj obj{};
  mempeep::ErrorTracer tracer{};
  CHECK(!mempeep::read(reader, 0, obj, tracer));
}