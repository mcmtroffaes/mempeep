#pragma once

#include <type_traits>

namespace mempeep {

/**
 * @brief Primitive types that be directly copied in memory.
 *
 * A type satisfies IsPrimitive if it can be copied byte-for-byte
 * (e.g. via memcpy) without requiring construction, destruction,
 * or pointer fixups.
 */
template <typename T>
concept IsPrimitive = std::is_trivially_copyable_v<T>;

}  // namespace mempeep