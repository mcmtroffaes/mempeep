#pragma once

#include "memory.hpp"

namespace mempeep {
	
template <typename T>
concept IsLayoutItem = requires { typename T::layout_item_tag; };

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
template <IsLayoutItem... Items>
struct Layout {};

/**
 * @brief Tag for registering the remote layout of a native struct.
 *
 * Example:
 *
 *   auto remote_layout(remote_layout_tag<Pos>)
 *     -> Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>>;
 *
 * @tparam T Native struct to register the layout for.
 */
template <typename T>
  requires IsReadable<T>
struct remote_layout_tag {};

/**
 * @brief Does T have a custom layout?
 *
 * Checks if the function remote_layout(remote_layout_tag<T>) exists.
 */
template <typename T>
concept HasRemoteLayout = requires { remote_layout(remote_layout_tag<T>{}); };

/**
 * @brief Shorthand for return type of `remote_layout(remote_layout_tag<T>{})`.
 */
template <typename T>
  requires HasRemoteLayout<T>
using remote_layout_t = decltype(remote_layout(remote_layout_tag<T>{}));

/**
 * @brief A field. Its type must be readable.
 *
 * For example, Field<&Class::member>.
 *
 * @tparam M The native field to deserialize into.
 */
template <auto M>
  requires IsReadable<member_type_t<M>>
struct Field {
  using layout_item_tag = void;
};

/**
 * @brief Padding relative to the current position in the layout.
 * @tparam N Number of bytes (strictly positive; zero is excluded as
 *           zero padding is never needed in practice).
 *           Its value must be representable by address_t<MemoryReader>.
 */
template <auto N>
  requires(std::in_range<std::size_t>(N) && N > 0)
struct Pad {
  using layout_item_tag = void;
  static constexpr std::size_t count = static_cast<std::size_t>(N);
};

/**
 * @brief Absolute offset relative to base position of the layout.
 *
 * Seeks are not required to be monotonically increasing. This allows
 * skipping around a non-linear layout. It is the caller's responsibility to
 * ensure the offsets are correct.
 *
 * @tparam N The offset in bytes (strictly positive; zero is excluded as
 *           jumping to base is never needed in practice).
 *           Its value must be representable by address_t<MemoryReader>.
 */
template <auto N>
  requires(std::in_range<std::size_t>(N) && N > 0)
struct Seek {
  using layout_item_tag = void;
  static constexpr std::size_t offset = static_cast<std::size_t>(N);
};

/**
 * @brief Read a remote address without following it, storing it in the member.
 *
 * Always reads exactly sizeof(address_type) bytes, not
 * sizeof(member_type_t<M>). The result is cast to the member type, which must
 * be wide enough to hold address_t<MemoryReader>, or a compile error results.
 *
 * @tparam M The native field to store the address into.
 */
template <auto M>
  requires IsAddress<member_type_t<M>>
struct RawAddr {
  using layout_item_tag = void;
};

/**
 * @brief Read a remote address and follow it, reading the pointee into the
 * member. The address itself is not stored.
 *
 * Errors if the address is null.
 *
 * @tparam M The native field to deserialize the pointee into.
 */
template <auto M>
  requires IsReadable<member_type_t<M>>
struct Ref {
  using layout_item_tag = void;
};

/**
 * @brief Read a remote address and follow it, reading the pointee into the
 * member. The address itself is not stored.
 *
 * If the address is null, the member is set to std::nullopt and no error
 * is reported.
 *
 * @tparam M The native field to deserialize the pointee into.
 *           Must have type std::optional<T> where T is readable.
 */
template <auto M>
  requires IsReadable<unwrap_optional_t<member_type_t<M>>>
struct NullableRef {
  using layout_item_tag = void;
};

}