#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mempeep/concepts/tracer.hpp>
#include <mempeep/tracers/ok_tracer.hpp>

using namespace mempeep;

static_assert(IsTracer<OkTracer>);
static_assert(!IsScopedTracer<OkTracer>);
static_assert(!IsDescScopedTracer<OkTracer>);
static_assert(!IsValueTracer<OkTracer>);

TEST_CASE("init") {
  OkTracer tracer{};
  CHECK(tracer.success());
}

TEST_CASE("error") {
  OkTracer tracer{};
  tracer.error(Error::READ_FAILED);
  CHECK(!tracer.success());
}