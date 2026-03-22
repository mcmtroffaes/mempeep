#include <cassert>
#include <iostream>  // std::cout, ...
#include <mempeep/read.hpp>
#include <mempeep/test/mock_game_data.hpp>

#include "log_tracer.hpp"

static_assert(mempeep::IsScopedTracer<LogTracer>);

int main() {
  uint16_t base{4};
  auto reader = mempeep::test::make_game_reader();
  mempeep::test::Game game{};
  LogTracer tracer{std::cout};
  assert(mempeep::read_remote(reader, base, game, tracer));
}
