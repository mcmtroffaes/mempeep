// not a doctest since we only need static_asserts
#include <cstddef>  // std::size_t
#include <cstdint>  // std::int32_t, ...
#include <mempeep/address.hpp>

static_assert(!mempeep::IsAddress<bool>);
static_assert(!mempeep::IsAddress<char>);
static_assert(!mempeep::IsAddress<short>);         // signed...
static_assert(!mempeep::IsAddress<int>);           // signed...
static_assert(!mempeep::IsAddress<std::int32_t>);  // signed...
static_assert(mempeep::IsAddress<unsigned short>);
static_assert(mempeep::IsAddress<unsigned int>);
static_assert(mempeep::IsAddress<unsigned long>);
static_assert(mempeep::IsAddress<unsigned long long>);
static_assert(mempeep::IsAddress<std::uint8_t>);
static_assert(mempeep::IsAddress<std::uint16_t>);
static_assert(mempeep::IsAddress<std::uint32_t>);
static_assert(mempeep::IsAddress<std::uint64_t>);
static_assert(mempeep::IsAddress<std::uintptr_t>);
static_assert(mempeep::IsAddress<std::uintmax_t>);
static_assert(mempeep::IsAddress<std::size_t>);

int main() { return 0; };
