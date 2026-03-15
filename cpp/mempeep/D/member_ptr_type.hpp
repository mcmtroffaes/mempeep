#pragma once

namespace mempeep::detail {

	// Default template: do not assign type.
	template<typename T>
	struct member_ptr_type;

	// Member pointer type specialization: extract underlying type of member.
	// "T C::*" is the type of a pointer to a member of the class C of type T.
	// For instance, "int Pos::*" is the type of &Pos::x if x is defined as an int member of Pos.
	// You can get this type using decltype(&Pos::x).
	template<typename C, typename T>
	struct member_ptr_type<T C::*> {
		using type = T;
	};

	// Shorthand for convenience.
	// member_ptr_type_t<MP> is a shorthand for member_ptr_type<MP>::type.
	// Only to be used if MP is a member pointer type.
	template<typename MP>
	using member_ptr_type_t =
		typename member_ptr_type<MP>::type;

}