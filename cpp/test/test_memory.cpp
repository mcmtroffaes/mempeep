// not a doctest since we only need static_asserts
#include <cstdint>
#include <mempeep/memory.hpp>

struct Struct1 {};

template<typename T>
struct Struct2 {
  using address_type = T;
};

template <typename T>
struct Struct3 {
  using address_type = T;

  bool operator()(T, std::size_t, void*) { return false; };
};

static_assert(!mempeep::IsMemoryReader<Struct1>);
static_assert(!mempeep::IsMemoryReader<Struct2<uint32_t>>);
static_assert(!mempeep::IsMemoryReader<Struct3<int32_t>>);  // signed -> no...
static_assert(mempeep::IsMemoryReader<Struct3<uint32_t>>);

int main() { return 0; };
