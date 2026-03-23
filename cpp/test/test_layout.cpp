#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mempeep/layout.hpp>
#include <optional>
#include <vector>

struct PrimTriv {
  using is_primitive_tag = void;
};

struct RemTriv {
  using remote_layout = mempeep::Layout<>;
};

struct SomeStruct {
  uint8_t addr1;
  uint16_t addr2;
  uint32_t addr3;
  uint64_t addr4;
  std::vector<int> x;
  int y;
  std::optional<int> z;
};

static_assert(mempeep::IsPrimitive<bool>);
static_assert(mempeep::IsPrimitive<char>);
static_assert(mempeep::IsPrimitive<short>);
static_assert(mempeep::IsPrimitive<int>);
static_assert(mempeep::IsPrimitive<long>);
static_assert(mempeep::IsPrimitive<long long>);
static_assert(mempeep::IsPrimitive<float>);
static_assert(mempeep::IsPrimitive<double>);
static_assert(mempeep::IsPrimitive<long double>);
static_assert(mempeep::IsPrimitive<wchar_t>);
static_assert(mempeep::IsPrimitive<char8_t>);
static_assert(mempeep::IsPrimitive<char16_t>);
static_assert(mempeep::IsPrimitive<char32_t>);
static_assert(mempeep::IsPrimitive<int8_t>);
static_assert(mempeep::IsPrimitive<int16_t>);
static_assert(mempeep::IsPrimitive<int32_t>);
static_assert(mempeep::IsPrimitive<int64_t>);
static_assert(mempeep::IsPrimitive<intptr_t>);
static_assert(mempeep::IsPrimitive<intmax_t>);
static_assert(mempeep::IsPrimitive<uint8_t>);
static_assert(mempeep::IsPrimitive<uint16_t>);
static_assert(mempeep::IsPrimitive<uint32_t>);
static_assert(mempeep::IsPrimitive<uint64_t>);
static_assert(mempeep::IsPrimitive<uintptr_t>);
static_assert(mempeep::IsPrimitive<uintmax_t>);
static_assert(mempeep::IsPrimitive<size_t>);
static_assert(mempeep::IsPrimitive<ptrdiff_t>);
static_assert(mempeep::IsPrimitive<PrimTriv>);
static_assert(!mempeep::IsPrimitive<std::optional<int>>);
static_assert(!mempeep::IsPrimitive<std::array<int, 5>>);
static_assert(!mempeep::IsPrimitive<std::vector<int>>);
static_assert(!mempeep::IsPrimitive<RemTriv>);
static_assert(!mempeep::IsPrimitive<SomeStruct>);

static_assert(!mempeep::HasRemoteLayout<PrimTriv>);
static_assert(!mempeep::HasRemoteLayout<SomeStruct>);
static_assert(mempeep::HasRemoteLayout<RemTriv>);

static_assert(mempeep::IsLayoutItem<mempeep::Pad<0>>);
static_assert(mempeep::IsLayoutItem<mempeep::Seek<0>>);
static_assert(mempeep::IsLayoutItem<mempeep::Field<&SomeStruct::y>>);
static_assert(mempeep::IsLayoutItem<mempeep::RawAddr<&SomeStruct::addr1>>);
static_assert(mempeep::IsLayoutItem<mempeep::RawAddr<&SomeStruct::addr2>>);
static_assert(mempeep::IsLayoutItem<mempeep::RawAddr<&SomeStruct::addr3>>);
static_assert(mempeep::IsLayoutItem<mempeep::RawAddr<&SomeStruct::addr4>>);
static_assert(mempeep::IsLayoutItem<mempeep::Ref<&SomeStruct::y>>);
static_assert(mempeep::IsLayoutItem<mempeep::NullableRef<&SomeStruct::z>>);

static_assert(mempeep::IsReadable<PrimTriv>);
static_assert(mempeep::IsReadable<RemTriv>);
static_assert(!mempeep::IsReadable<SomeStruct>);