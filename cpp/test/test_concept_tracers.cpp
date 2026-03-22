#include <mempeep/tracer.hpp>
#include <mempeep/tracers.hpp>

static_assert(IsTracer<ErrorTracer>);
static_assert(!IsScopedTracer<ErrorTracer>);
static_assert(IsScopedTracer<LogTracer>);

int main() { return 0; }