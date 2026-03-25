// not a doctest since we only need static_asserts
#include <mempeep/layout.hpp>
#include <optional>
#include <vector>

struct PrimTriv {};

struct SomeStruct {
  uint8_t addr1;
  uint16_t addr2;
  uint32_t addr3;
  uint64_t addr4;
  std::vector<int> x;
  int y;
  std::optional<int> z;
};

using namespace mempeep;

static_assert(IsPrimitive<bool>);
static_assert(IsPrimitive<char>);
static_assert(IsPrimitive<short>);
static_assert(IsPrimitive<int>);
static_assert(IsPrimitive<long>);
static_assert(IsPrimitive<long long>);
static_assert(IsPrimitive<unsigned char>);
static_assert(IsPrimitive<unsigned short>);
static_assert(IsPrimitive<unsigned int>);
static_assert(IsPrimitive<unsigned long>);
static_assert(IsPrimitive<unsigned long long>);
static_assert(IsPrimitive<float>);
static_assert(IsPrimitive<double>);
static_assert(IsPrimitive<long double>);
static_assert(IsPrimitive<wchar_t>);
static_assert(IsPrimitive<char8_t>);
static_assert(IsPrimitive<char16_t>);
static_assert(IsPrimitive<char32_t>);
static_assert(IsPrimitive<int8_t>);
static_assert(IsPrimitive<int16_t>);
static_assert(IsPrimitive<int32_t>);
static_assert(IsPrimitive<int64_t>);
static_assert(IsPrimitive<intptr_t>);
static_assert(IsPrimitive<intmax_t>);
static_assert(IsPrimitive<uint8_t>);
static_assert(IsPrimitive<uint16_t>);
static_assert(IsPrimitive<uint32_t>);
static_assert(IsPrimitive<uint64_t>);
static_assert(IsPrimitive<uintptr_t>);
static_assert(IsPrimitive<uintmax_t>);
static_assert(IsPrimitive<size_t>);
static_assert(IsPrimitive<ptrdiff_t>);
static_assert(IsPrimitive<PrimTriv>);
static_assert(IsPrimitive<std::optional<int>>);  // it is!
static_assert(IsPrimitive<std::array<int, 5>>);  // it is!
static_assert(!IsPrimitive<std::vector<int>>);
static_assert(!IsPrimitive<SomeStruct>);

static_assert(IsFieldsItem<Pad<0>>);
static_assert(IsFieldsItem<Seek<0>>);
static_assert(IsFieldsItem<Field<Primitive<int>, &SomeStruct::y>>);
static_assert(IsFieldsItem<Field<RawAddr<uint8_t>, &SomeStruct::addr1>>);
static_assert(IsFieldsItem<Field<RawAddr<uint16_t>, &SomeStruct::addr2>>);
static_assert(IsFieldsItem<Field<RawAddr<uint32_t>, &SomeStruct::addr3>>);
static_assert(IsFieldsItem<Field<RawAddr<uint64_t>, &SomeStruct::addr4>>);
static_assert(IsFieldsItem<Field<Ref<Primitive<int>>, &SomeStruct::y>>);
static_assert(
  IsFieldsItem<Field<NullableRef<Primitive<int>>, &SomeStruct::z>>
);

int main() { return 0; };
