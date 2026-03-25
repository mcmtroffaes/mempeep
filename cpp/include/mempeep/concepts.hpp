#pragma once

#include <concepts>

namespace mempeep {

/**
 * @brief Possible user address types.
 */
template <typename T>
concept IsAddress = std::unsigned_integral<T> && !std::same_as<T, bool>;

/**
 * @brief Does T have fields?
 *
 * Checks if `T::fields` exists.
 */
template <typename T>
concept IsStruct = requires { typename T::fields; };

/**
 * @brief Shorthand for `T::fields`.
 */
template <typename T>
  requires IsStruct<T>
using fields_t = typename T::fields;

/** @brief Primitive types that be directly copied in memory.
 *
 * A type satisfies IsPrimitive if it can be copied byte-for-byte
 * (e.g. via memcpy) without requiring construction, destruction,
 * or pointer fixups.
 *
 * For conceptual clarity, IsStruct is excluded from IsPrimitive
 * since for IsStruct we should use `T::fields` to deserialize
 * instead of copying byte-for-byte.
 */
template <typename T>
concept IsPrimitive = std::is_trivially_copyable_v<T> && !IsStruct<T>;

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
 * @brief Shorthand for `T::native_type`.
 */
template <typename T>
  requires IsDescriptor<T>
using native_type_t = typename T::native_type;

}  // namespace mempeep