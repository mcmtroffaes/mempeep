// not a doctest since we only need static_asserts
#include <concepts>  // std::same_as
#include <mempeep/detail/member_traits.hpp>

struct Obj {
  int* c;
};

static_assert(std::same_as<mempeep::detail::member_class_t<&Obj::c>, Obj>);
static_assert(std::same_as<mempeep::detail::member_type_t<&Obj::c>, int*>);

int main() { return 0; };
