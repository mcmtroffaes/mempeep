#pragma once

#include <cstddef>

namespace mempeep::D {

	template <std::size_t N>
	struct Pad {
		static_assert(N > 0);
		static constexpr std::size_t size = N;
	};

	template <std::size_t N>
	struct Offset {
		static constexpr std::size_t n = N;
	};

	// MemberPtr tells us where the value is to be stored in host memory after reading.
	// T tells us how the value is stored in the target memory.
	template <auto MemberPtr, typename T = void>
	struct Field {
		static constexpr auto member = MemberPtr;
		using Type = T;  // void extracts type from MemberPtr
	};

	// Specialise for every struct you want to interact with.
	// Must provide:
	//   using fields = std::tuple<...>  - Field<>, Pad<>, and Offset<> descriptors
	// The member pointer Field<> should be a member pointer of S.
	// Pad<> and Offset<> can be used to describe gaps in the memory layout.
	template <typename S>
	struct Struct;

} // namespace mempeep::D
