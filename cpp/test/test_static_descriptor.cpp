// not a doctest since we only need static_asserts
#include <concepts>
#include <mempeep/descriptor.hpp>

using namespace mempeep;

using Int = Primitive<int>;

static_assert(!IsDescriptor<void>);
static_assert(!IsDescriptor<std::vector<int>>);
static_assert(IsDescriptor<Int>);
static_assert(IsDescriptor<Ref<Int>>);
static_assert(IsDescriptor<NullableRef<Int>>);
static_assert(IsDescriptor<Array<Int, 10>>);
static_assert(IsDescriptor<Vector<Int, 0x1000>>);
static_assert(IsDescriptor<Array<Vector<Ref<Int>, 0x1000>, 23>>);

int main() { return 0; };
