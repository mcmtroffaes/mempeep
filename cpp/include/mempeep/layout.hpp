#pragma once

#include <cstddef>              // std::size_t
#include <mempeep/concepts.hpp>  // IsAddress
#include <mempeep/detail/traits.hpp>   // member_type_t, ...

namespace mempeep {

template <typename T>
concept IsFieldsItem = requires { typename T::fields_item_tag; };

/**
 * @brief Defines a remote layout.
 *
 * Each item must be a Field, Pad, or Seek.
 * Example:
 *
 *   Fields<Field<&Pos::x>, Pad<4>, Field<&Pos::y>>
 *
 * @tparam Items Sequence of Field, Seek, and Pad types.
 */
template <IsFieldsItem... Items>
struct Fields {};

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
  requires IsReadable<detail::member_type_t<M>>
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
  requires IsAddress<detail::member_type_t<M>>
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
  requires IsReadable<detail::member_type_t<M>>
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
  requires IsReadable<detail::unwrap_optional_t<detail::member_type_t<M>>>
struct NullableRef {
  using fields_item_tag = void;
};

template <auto M>
  requires IsReadable<detail::unwrap_array_t<detail::member_type_t<M>>>
struct Array {
  using fields_item_tag = void;
};

template <auto M>
  requires IsReadable<detail::unwrap_vector_t<detail::member_type_t<M>>>
struct Vector {
  using fields_item_tag = void;
};

template <auto M, auto N, std::size_t L>
  requires IsReadable<detail::unwrap_vector_t<detail::member_type_t<M>>>
           && IsAddress<detail::member_type_t<N>>
struct CircularList {
  using fields_item_tag = void;
};

}  // namespace mempeep