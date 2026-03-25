// not a doctest since we only need static_asserts
#include <concepts>  // std::same_as
#include <mempeep/detail/traits.hpp>

struct Struct {
  int* c;
};

static_assert(std::same_as<mempeep::detail::member_class_t<&Struct::c>, Struct>);
static_assert(std::same_as<mempeep::detail::member_type_t<&Struct::c>, int*>);
static_assert(std::same_as<
              mempeep::detail::unwrap_optional_t<std::optional<char**>>,
              char**>);

int main() { return 0; };
