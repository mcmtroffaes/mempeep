// not a doctest since we only need static_asserts
#include <cstddef>  // std::size_t
#include <cstdint>  // std::int32_t, ...
#include <mempeep/concepts/memory.hpp>

struct Obj1 {};

template <typename T>
struct Obj2 {
  using address_type = T;
};

template <typename T>
struct Obj3 {
  using address_type = T;

  bool operator()(T, std::size_t, void*) { return false; };
};

static_assert(!mempeep::IsMemoryReader<Obj1>);
static_assert(!mempeep::IsMemoryReader<Obj2<std::uint32_t>>);
static_assert(!mempeep::IsMemoryReader<Obj3<bool>>);
static_assert(!mempeep::IsMemoryReader<Obj3<char>>);
static_assert(!mempeep::IsMemoryReader<Obj3<short>>);         // signed...
static_assert(!mempeep::IsMemoryReader<Obj3<int>>);           // signed...
static_assert(!mempeep::IsMemoryReader<Obj3<std::int32_t>>);  // signed...
static_assert(mempeep::IsMemoryReader<Obj3<std::uint8_t>>);
static_assert(mempeep::IsMemoryReader<Obj3<std::uint16_t>>);
static_assert(mempeep::IsMemoryReader<Obj3<std::uint32_t>>);
static_assert(mempeep::IsMemoryReader<Obj3<std::uint64_t>>);
static_assert(mempeep::IsMemoryReader<Obj3<std::uintptr_t>>);
static_assert(mempeep::IsMemoryReader<Obj3<std::uintmax_t>>);
static_assert(mempeep::IsMemoryReader<Obj3<std::size_t>>);

int main() { return 0; };
