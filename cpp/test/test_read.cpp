#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <iostream>  // std::cout, ...
#include <mempeep/read.hpp>

#include "support/mock_game_data.hpp"

TEST_CASE("read") {
  uint16_t base{4};
  auto reader = mempeep::test::make_game_reader();
  mempeep::test::Game game{};
  mempeep::ErrorTracer tracer{};
  CHECK(mempeep::read(reader, base, game, tracer));
  mempeep::test::check_game(game);
}
