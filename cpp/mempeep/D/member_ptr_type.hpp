#pragma once

namespace mempeep::detail {

	// Default template: do not assign type.
	template<typename T>
	struct member_ptr_type;

	// Member pointer type specialization: extract underlying type of member.
	// "Type Class::*" is the type of a pointer to a member of type Type in class Class.
	// For instance, "int Pos::*" is the type of &Pos::x if x is defined as an int member of Pos.
	// You can get this type using decltype(&Pos::x).
	template<typename Class, typename MemberType>
	struct member_ptr_type<MemberType Class::*> {
		using type = MemberType;
	};

	// Shorthand.
	// member_ptr_type_t<T> is a shorthand for member_ptr_type<T>::type.
	// Only use if T is a member pointer type, such as decltype(&Pos::x).
	template<typename T>
	using member_ptr_type_t =
		typename member_ptr_type<T>::type;

}