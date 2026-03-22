#pragma once

#include <concepts>

namespace mempeep {

/** @brief Possible user address types. */
template <typename T>
concept IsAddress = std::unsigned_integral<T> && !std::same_as<T, bool>
                    && !std::same_as<T, char>;

}  // namespace mempeep