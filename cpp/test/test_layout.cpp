#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mempeep/layout.hpp>
#include <vector>

struct Prim {
  using is_primitive_tag = void;
  int x;
};

struct NonPrim {
  std::vector<int> x;
};

static_assert(mempeep::IsPrimitive<int>);
static_assert(mempeep::IsPrimitive<Prim>);
static_assert(!mempeep::IsPrimitive<std::optional<int>>);
static_assert(!mempeep::IsPrimitive<std::array<int, 5>>);
static_assert(!mempeep::IsPrimitive<std::vector<int>>);
static_assert(!mempeep::IsPrimitive<NonPrim>);

static_assert(mempeep::IsLayoutItem<mempeep::Field<&NonPrim::x>>);

TEST_CASE("trivial") {}