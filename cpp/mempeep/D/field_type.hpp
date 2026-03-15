#pragma once

#include <type_traits>  // is_void_v

#include "mempeep/D.hpp"
#include "mempeep/D/member_ptr_type.hpp"

namespace mempeep::detail {

	// Default template: do not assign type.
	template<typename T>
	struct field_type;

	// Field specialization: assign T if not void,
	// or otherwise extract the underlying type from the member pointer M.
	template<auto MemberPtr, typename T>
	struct field_type<D::Field<MemberPtr, T>> {
		using type = std::conditional_t<
			std::is_void_v<T>,
			// if T = void, return type of member pointer
			// Remember M is here the member pointer itself, not the type of the member pointer.
			// So we need decltype(M) as member_ptr_type_t expects the type of the member pointer.
			member_ptr_type_t<decltype(MemberPtr)>,
			// otherwise return T
			T
		>;
	};

	// Shorthand.
	// field_type_t<T> is a shorthand for field_type<T>::type.
	// Only use if T is a member pointer, such as &Pos::x.
	template<typename T>
	using field_type_t = typename field_type<T>::type;

}