#pragma once

#include <mempeep/tracer.hpp>
#include <ostream>

/** @brief Simple scoped tracer.
 * 
 * Logs read operations throughout the layout.
 * Logs errors too.
 * Tracks if an error happened at any point.
 */
struct LogTracer {
  std::ostream& out;
  bool ok = true;
  int indent = 0;
  uint64_t address = 0;

  void log(std::string_view msg) {
    const auto whitespace = std::string(indent, ' ');
    out << std::format("[{:08X}] ", address) << whitespace << msg << std::endl;
  }

  void error(std::string_view reason) {
    ok = false;
    log(std::format("ERROR: {}", reason));
  }

  bool success() const { return ok; }

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

static_assert(mempeep::IsScopedTracer<LogTracer>);
