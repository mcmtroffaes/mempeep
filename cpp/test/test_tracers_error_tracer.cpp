#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mempeep/concepts/tracer.hpp>
#include <mempeep/tracers/error_tracer.hpp>

using namespace mempeep;

static_assert(IsTracer<ErrorTracer>);
static_assert(!IsScopedTracer<ErrorTracer>);
static_assert(!IsDescScopedTracer<ErrorTracer>);
static_assert(!IsValueTracer<ErrorTracer>);

TEST_CASE("init") {
  ErrorTracer tracer{};
  CHECK(tracer.success());
}

TEST_CASE("error") {
  ErrorTracer tracer{};
  tracer.error(Error::READ_FAILED);
  CHECK(!tracer.success());
}