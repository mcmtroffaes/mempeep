#pragma once

namespace mempeep {

/**
 * @brief Concept satisfied by all descriptor types.
 *
 * A descriptor describes how to read a value from remote memory and what
 * native type it produces. Every descriptor exposes a `native_type` alias
 * giving the native type it populates.
 */
template <typename Desc>
concept IsDescriptor = requires { typename Desc::native_type; };

/**
 * @brief Shorthand for `typename T::native_type`.
 */
template <typename T>
  requires IsDescriptor<T>
using native_type_t = typename T::native_type;

}  // namespace mempeep