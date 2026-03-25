#pragma once

#include <cstddef>              // std::size_t
#include <mempeep/address.hpp>  // IsAddress
#include <mempeep/traits.hpp>   // member_type_t, ...

namespace mempeep {

/** @brief Primitive types that be directly copied in memory.
 *
 * A type satisfies IsPrimitive if it can be copied byte-for-byte
 * (e.g. via memcpy) without requiring construction, destruction,
 * or pointer fixups.
 *
 * This includes all numeric types, and all types tagged with the
 * `is_primitive_tag`.
 *
 * @warning
 * Incorrectly tagging a type as primitive may lead to undefined behavior.
 *
 * Example:
 * @code
 * struct Pos {
 *   using is_primitive_tag = void;
 *   float x, y, z;
 * }
 * @endcode
 *
 * Constrained on layout items so failures are caught at layout
 * definition time, not at memory reading time.
 */
template <typename T>
concept IsPrimitive = requires { typename T::is_primitive_tag; }
                      || std::integral<T> || std::floating_point<T>;

template <typename T>
concept IsFieldsItem = requires { typename T::fields_item_tag; };

/**
 * @brief Defines a remote layout.
 *
 * Each item must be a Field, Pad, or Seek.
 * Example:
 *
 *   Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>>
 *
 * @tparam Items Sequence of Field, Seek, and Pad types.
 */
template <IsFieldsItem... Items>
struct Layout {};

/**
 * @brief Does T have a custom layout?
 *
 * Checks if the function fields(fields_tag<T>) exists.
 */
template <typename T>
concept IsStruct = requires { typename T::fields; };

/**
 * @brief Shorthand for return type of `fields(fields_tag<T>{})`.
 */
template <typename T>
  requires IsStruct<T>
using fields_t = typename T::fields;

template <typename T>
concept IsReadable = IsPrimitive<T> || IsStruct<T>;

/**
 * @brief A field. Its type must be readable.
 *
 * For example, Field<&Class::member>.
 *
 * @tparam M The field to deserialize into.
 */
template <auto M>
  requires IsReadable<member_type_t<M>>
struct Field {
  using fields_item_tag = void;
};

/**
 * @brief Padding relative to the current position in the layout.
 * @tparam N Number of bytes.
 *           Its value must be representable by address_t<MemoryReader>.
 */
template <auto N>
  requires(std::in_range<std::size_t>(N))
struct Pad {
  using fields_item_tag = void;
  static constexpr std::size_t count = static_cast<std::size_t>(N);
};

/**
 * @brief Absolute offset relative to base position of the layout.
 *
 * Seeks are not required to be monotonically increasing. This allows
 * skipping around a non-linear layout. It is the caller's responsibility to
 * ensure the offsets are correct.
 *
 * @tparam N The offset in bytes.
 *           Its value must be representable by address_t<MemoryReader>.
 */
template <auto N>
  requires(std::in_range<std::size_t>(N))
struct Seek {
  using fields_item_tag = void;
  static constexpr std::size_t offset = static_cast<std::size_t>(N);
};

/**
 * @brief Read a remote address without following it, storing it in the member.
 *
 * Always reads exactly sizeof(address_type) bytes, not
 * sizeof(member_type_t<M>). The result is cast to the member type, which must
 * be wide enough to hold address_t<MemoryReader>, or a compile error results.
 *
 * @tparam M The field to store the address into.
 */
template <auto M>
  requires IsAddress<member_type_t<M>>
struct RawAddr {
  using fields_item_tag = void;
};

/**
 * @brief Read a remote address and follow it, reading the pointee into the
 * member. The address itself is not stored.
 *
 * Errors if the address is null.
 *
 * @tparam M The field to deserialize the pointee into.
 */
template <auto M>
  requires IsReadable<member_type_t<M>>
struct Ref {
  using fields_item_tag = void;
};

/**
 * @brief Read a remote address and follow it, reading the pointee into the
 * member. The address itself is not stored.
 *
 * If the address is null, the member is set to std::nullopt and no error
 * is reported.
 *
 * @tparam M The field to deserialize the pointee into.
 *           Must have type std::optional<T> where T is readable.
 */
template <auto M>
  requires IsReadable<unwrap_optional_t<member_type_t<M>>>
struct NullableRef {
  using fields_item_tag = void;
};

template <auto M>
  requires IsReadable<unwrap_array_t<member_type_t<M>>>
struct Array {
  using fields_item_tag = void;
};

template <auto M>
  requires IsReadable<unwrap_vector_t<member_type_t<M>>>
struct Vector {
  using fields_item_tag = void;
};

template <auto M, auto N, std::size_t L>
  requires IsReadable<unwrap_vector_t<member_type_t<M>>> && IsAddress<member_type_t<N>>
struct CircularList {
  using fields_item_tag = void;
};

}  // namespace mempeep