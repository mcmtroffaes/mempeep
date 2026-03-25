#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mempeep/read.hpp>
#include <optional>

#include "log_tracer.hpp"
#include "support/buffer_reader.hpp"

using namespace mempeep;

// --- Types ---

struct Pos {
  int8_t x, y;

  // 0: x
  // 1: pad
  // 2: y
  // 3: pad
  using fields = Layout<Field<&Pos::x>, Pad<1>, Field<&Pos::y>, Pad<1>>;
};

struct Entity {
  int8_t id;
  Pos pos;
  uint8_t target_addr;         // raw address, not followed
  Pos extra_pos;               // followed non-nullable address to Pos
  std::optional<Pos> opt_pos;  // followed nullable address to Pos

  // 0-1: header
  // 2:   id
  // 3:   pad
  // 4-7: pos
  // 8:   target_addr (raw address)
  // 9:   extra_pos (read address, follow)
  // 10:  opt_pos (read address, follow if non-null)
  using fields = Layout<
    Seek<2>,
    Field<&Entity::id>,
    Pad<1>,
    Field<&Entity::pos>,
    RawAddr<&Entity::target_addr>,
    Ref<&Entity::extra_pos>,
    NullableRef<&Entity::opt_pos>>;
};

static void report(bool ok, const Entity& e) {
  std::cout << "-- decoded (success=" << std::boolalpha << ok << ") --\n";
  std::cout << "id:          " << (int)e.id << "\n";
  std::cout << "pos.x:       " << (int)e.pos.x << "\n";
  std::cout << "pos.y:       " << (int)e.pos.y << "\n";
  std::cout << "target_addr: " << (int)e.target_addr << "\n";
  std::cout << "extra_pos.x: " << (int)e.extra_pos.x << "\n";
  std::cout << "extra_pos.y: " << (int)e.extra_pos.y << "\n";
  std::cout << "opt_pos set: " << std::boolalpha << e.opt_pos.has_value()
            << "\n";
  if (e.opt_pos.has_value()) {
    std::cout << "opt_pos.x: " << (int)e.opt_pos->x << "\n";
    std::cout << "opt_pos.y: " << (int)e.opt_pos->y << "\n";
  }
}

template <std::size_t N>
static void read_entity(const char (&data)[N]) {
  BufferReader reader{data};
  Entity e{};
  LogTracer tracer{std::cout};
  const bool ok = mempeep::read(reader, uint8_t{0}, e, tracer);
  report(ok, e);
}

int main() {
  // Play around with the values to see what happens.
  read_entity(
    "\x13\x37"          // 0-1:   header bytes
    "\x07"              // 2:     id = 7
    "\x00"              // 3:     Pad<1>
    "\x03\x00\xfe\x00"  // 4-7:   pos x=3, pad, y=-2, pad
    "\x0b"              // 8:     target_addr -> offset 0b
    "\x0f"              // 9:     extra_pos addr -> offset 0f
    "\x00"              // 0a:    opt_pos addr = null
    "\x00\x00\x00\x00"  // 0b-0e: gap
    "\x05\x00\xff\x00"  // 0f-12: extra_pos x=5, pad, y=-1, pad
  );
}
