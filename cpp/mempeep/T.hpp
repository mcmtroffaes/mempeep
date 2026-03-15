#pragma once

// For mempeep, a "valid type" is any of the following:
//   * A fixed-sized data type (int32_t, etc.).
//   * A struct containing any of the above.
//   * A std::array containing any of the above.
//   * A std::vector containing any of the above.
//     Expects a begin/end pointer pair in memory.
//   * A Ptr. Expects a pointer to a value. The value returned is a follows:
//       - Default: T (i.e. the pointee)
//       - Weak: T* (i.e. the pointer; never null)
//       - Optional: std::optional<T> (i.e. optional pointee)
//       - Optional and weak: T* (i.e. the pointer; can be null)
//   * Any CircularList containing any of the above.
//     Expects a head pointer to the first value in memory.
//     Head pointer can be null; this means the list is empty.
//     Next pointers are never null.

namespace mempeep::T {

	enum PtrFlags {
		None = 0,
		Optional = 1 << 0,
		Weak = 1 << 1
	};

	template <typename T, int Flags = None>
	struct Ptr {
		using Type = T;
		static constexpr bool is_optional = Flags & Optional;
		static constexpr bool is_weak = Flags & Weak;
	};

	template <typename T>
	struct CircularList {
		using Type = T;
	};

}  // namespace mempeep::T
