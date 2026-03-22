#include <iostream>
#include <mempeep/read.hpp>

#include "log_tracer.hpp"
#include "support/buffer_reader.hpp"

using namespace mempeep;

struct Vec2 {
  int x, y;
};

auto remote_layout(remote_layout_tag<Vec2>)
  -> Layout<Field<&Vec2::x>, Pad<4>, Field<&Vec2::y>>;

int main() {
  BufferReader reader{
    "\x01\x00\x00\x00"  // x = 1
    "\x00\x00\x00\x00"  // pad
    "\x02\x00\x00\x00"  // y = 2
  };

  Vec2 v{};
  LogTracer tracer{std::cout};
  mempeep::read(reader, 0, v, tracer);
}