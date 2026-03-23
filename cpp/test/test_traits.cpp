// not a doctest since we only need static_asserts
#include <concepts>  // std::same_as
#include <mempeep/traits.hpp>

struct Struct {
  int* c;
};

static_assert(std::same_as<mempeep::member_type_t<&Struct::c>, int*>);
static_assert(
  std::same_as<mempeep::unwrap_optional_t<std::optional<char**>>, char**>
);

int main() { return 0; };
