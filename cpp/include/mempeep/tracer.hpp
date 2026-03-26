#pragma once

#include <concepts>           // std::same_as, std::convertible_to
#include <cstdint>            // std::uint64_t
#include <mempeep/error.hpp>  // Error

namespace mempeep {

/**
 * @brief Observes errors and summarises whether any occurred.
 *
 * The minimal tracer interface. Richer behaviour is available via the
 * optional extensions IsScopedTracer and IsValueTracer.
 *
 * error() is called once per error encountered during a read. It receives
 * an Error value describing what went wrong. The same error code may be
 * reported multiple times if multiple reads fail.
 *
 * success() is called once at the end of a read to determine the return
 * value of mempeep::read(). By convention it returns true if no errors
 * were reported and false otherwise, but callers should not assume this:
 * a tracer may implement custom logic, for example treating certain error
 * codes as non-fatal.
 */
template <typename Tracer>
concept IsTracer = requires(Tracer& tracer, Error e) {
  { tracer.error(e) } -> std::same_as<void>;
  { tracer.success() } -> std::convertible_to<bool>;
};

/**
 * @brief Optional extension of IsTracer that supports per-field-item scope
 * tracking.
 *
 * A tracer implementing this concept will have a Scope object constructed
 * at the start of each field item read and destroyed at the end, allowing
 * it to track nesting, log addresses, and label reads.
 *
 * Scope must be constructible as:
 * @code
 *   Scope(tracer, address, item)
 * @endcode
 * where address is std::uint64_t and item is a field item tag value
 * (Field<M>, Pad<N>, Seek<N>, ...).
 *
 * The concept only checks that Scope exists as a member type.
 * If Scope is not constructible with a given item type, no scope is created
 * for that item type and no overhead is incurred.
 */
template <typename Tracer>
concept IsScopedTracer
  = IsTracer<Tracer> && requires { typename Tracer::Scope; };

/**
 * @brief Optional extension of IsTracer that supports per-primitive-read
 * value reporting.
 *
 * A tracer implementing this concept will have its value() method called
 * after each successful primitive read,
 * allowing it to log or process the value alongside the address and layout
 * item provided by IsScopedTracer::Scope.
 *
 * value() must be callable as:
 * @code
 *   tracer.value(v)
 * @endcode
 * where v is any trivially copyable type without a remote layout.
 *
 * The concept only checks that value() exists for int; for other
 * primitive types, if value(v) is not callable, no value is reported
 * and no overhead is incurred.
 */
template <typename Tracer>
concept IsValueTracer = IsTracer<Tracer> && requires(Tracer& tracer) {
  { tracer.value(0) } -> std::same_as<void>;
};

/**
 * @brief Minimal IsTracer implementation that reports whether any error
 * occurred. Suitable when error details are not needed.
 */
struct ErrorTracer {
  bool ok = true;

  void error(Error) { ok = false; }

  bool success() const { return ok; }
};

static_assert(IsTracer<ErrorTracer>);
static_assert(!IsScopedTracer<ErrorTracer>);
static_assert(!IsValueTracer<ErrorTracer>);

}  // namespace mempeep