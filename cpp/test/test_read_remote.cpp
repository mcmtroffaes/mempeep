#include <cassert>
#include <iostream>  // std::cout, ...
#include <mempeep/read.hpp>

#include "support/mock_game_data.hpp"

int main() {
  uint16_t base{4};
  auto reader = mempeep::test::make_game_reader();
  mempeep::test::Game game{};
  mempeep::ErrorTracer tracer{};
  assert(mempeep::read_remote(reader, base, game, tracer));
}
