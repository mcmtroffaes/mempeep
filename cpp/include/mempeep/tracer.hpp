#pragma once

#include <cstdint>  // std::uint64_t
#include <string_view>
#include <type_traits>

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

}  // namespace mempeep