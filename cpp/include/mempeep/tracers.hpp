#pragma once

#include <iostream>
#include <mempeep/tracer.hpp>

namespace mempeep {

// Minimal tracer that detects if any error has occurred
struct ErrorTracer {
  bool ok = true;

  void error(std::string_view) { ok = false; }

  bool success() const { return ok; }
};

// Simple scoped tracer which prints errors along with the address where they
// occurred.
struct LogTracer : ErrorTracer {
  std::ostream& out;
  int indent = 0;
  uint64_t address = 0;

  explicit LogTracer(std::ostream& out = std::cout) : out(out) {}

  void log(std::string_view msg) {
    const auto whitespace = std::string(indent, ' ');
    out << std::format("[{:08X}] ", address) << whitespace << msg << std::endl;
  }

  void error(std::string_view reason) {
    ErrorTracer::error(reason);  // set flag
    log(std::format("ERROR: {}", reason));
  }

  struct Scope {
    LogTracer& t;

    Scope(LogTracer& _t, uint64_t address, std::string_view label) : t(_t) {
      t.address = address;
      t.log(label);
      t.indent++;
    }

    ~Scope() { t.indent--; }
  };
};

static_assert(IsTracer<ErrorTracer>);
static_assert(!IsScopedTracer<ErrorTracer>);
static_assert(IsScopedTracer<LogTracer>);

}  // namespace mempeep