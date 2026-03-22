#pragma once

#include <cstdint>      // std::uint64_t
#include <string_view>  // std::string_view
#include <type_traits>  // std::convertible_to

namespace mempeep {

// note: uint64_t address avoids dependence on MemoryReader
// this is ok for logging, and keeps implementation simple
template <typename Tracer>
concept IsTracer = requires(Tracer& tracer, std::string_view s) {
  { tracer.error(s) } -> std::same_as<void>;          // report error
  { tracer.success() } -> std::convertible_to<bool>;  // any error reported?
};

template <typename Tracer>
concept IsScopedTracer
  = IsTracer<Tracer>
    && requires(Tracer& tracer, std::uint64_t addr, std::string_view s) {
         typename Tracer::Scope;
         { typename Tracer::Scope(tracer, addr, s) };
       };

template <typename Tracer>
concept IsValueTracer = IsTracer<Tracer> && requires(Tracer& tracer) {
  { tracer.value(0) } -> std::same_as<void>;
};

// Minimal tracer that detects if any error has occurred
struct ErrorTracer {
  bool ok = true;

  void error(std::string_view) { ok = false; }

  bool success() const { return ok; }
};

static_assert(IsTracer<ErrorTracer>);
static_assert(!IsScopedTracer<ErrorTracer>);
static_assert(!IsValueTracer<ErrorTracer>);

}  // namespace mempeep