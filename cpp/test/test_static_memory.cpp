// not a doctest since we only need static_asserts
#include <cstddef>  // std::size_t
#include <cstdint>  // std::int32_t, ...
#include <mempeep/memory.hpp>

struct Struct1 {};

template <typename T>
struct Struct2 {
  using address_type = T;
};

template <typename T>
struct Struct3 {
  using address_type = T;

  bool operator()(T, std::size_t, void*) { return false; };
};

static_assert(!mempeep::IsMemoryReader<Struct1>);
static_assert(!mempeep::IsMemoryReader<Struct2<std::uint32_t>>);
static_assert(!mempeep::IsMemoryReader<Struct3<bool>>);
static_assert(!mempeep::IsMemoryReader<Struct3<char>>);
static_assert(!mempeep::IsMemoryReader<Struct3<short>>);         // signed...
static_assert(!mempeep::IsMemoryReader<Struct3<int>>);           // signed...
static_assert(!mempeep::IsMemoryReader<Struct3<std::int32_t>>);  // signed...
static_assert(mempeep::IsMemoryReader<Struct3<std::uint8_t>>);
static_assert(mempeep::IsMemoryReader<Struct3<std::uint16_t>>);
static_assert(mempeep::IsMemoryReader<Struct3<std::uint32_t>>);
static_assert(mempeep::IsMemoryReader<Struct3<std::uint64_t>>);
static_assert(mempeep::IsMemoryReader<Struct3<std::uintptr_t>>);
static_assert(mempeep::IsMemoryReader<Struct3<std::uintmax_t>>);
static_assert(mempeep::IsMemoryReader<Struct3<std::size_t>>);

int main() { return 0; };
