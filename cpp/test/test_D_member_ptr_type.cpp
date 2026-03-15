#include <type_traits>  // is_same_v

#include "mempeep/D/member_ptr_type.hpp"
#include "example.hpp"

using namespace mempeep;

static_assert(
	std::is_same_v<
	mempeep::detail::member_ptr_type_t<decltype(&Pos::x)>,
	int
	>
	);

static_assert(
	std::is_same_v<
	mempeep::detail::member_ptr_type_t<decltype(&Entity::pos)>,
	Pos
	>
	);

int main() {
	// test is passed if code compiles
	return 0;
}
