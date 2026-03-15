#include <type_traits>  // is_same_v

#include "mempeep/D/field_type.hpp"
#include "example.hpp"

using namespace mempeep;

static_assert(
	std::is_same_v<
	mempeep::detail::field_type_t<D::Field<&Pos::x> >,
	int
	>
	);

static_assert(
	std::is_same_v<
	mempeep::detail::field_type_t<D::Field<&Entity::pos> >,
	Pos
	>
	);

int main() {
	// test is passed if code compiles
	return 0;
}
