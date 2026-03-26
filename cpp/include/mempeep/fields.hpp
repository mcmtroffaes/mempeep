#pragma once

#include <concepts>  // std::same_as
#include <cstddef>   // std::size_t
#include <mempeep/concepts/descriptor.hpp>
#include <mempeep/concepts/fields.hpp>
#include <mempeep/detail/member_traits.hpp>

// In this file we set up everything for the Struct descriptor.
// The syntax is `Struct<T, Fields<Field<...>, Pad<...>, Seek<...>, ...>>`.
// So we need `Field`, `Pad`, `Seek`, and `Fields`.
// `Struct` is defined in `descriptor.hpp` along with the other descriptors.

namespace mempeep {

/**
 * @brief A field of a struct.
 *
 * Example: `Field<Primitive<int>, &X::x>`.
 *
 * @tparam Desc The descriptor (how it is stored in remote memory).
 * @tparam M    The field to deserialize into (where it is copied to natively).
 */
template <IsDescriptor Desc, auto M>
  requires std::same_as<native_type_t<Desc>, detail::member_type_t<M>>
struct Field {
  using fields_item_tag = void;
};

/**
 * @brief Padding relative to the current position in the layout.
 * @tparam N Number of bytes.
 *           Its value must be representable by address_t<MemoryReader>.
 */
template <std::size_t N>
struct Pad {
  using fields_item_tag = void;
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
template <std::size_t N>
struct Seek {
  using fields_item_tag = void;
};

/**
 * @brief Sequence of field items.
 *
 * @tparam Items The field items, mapping the memory layout.
 */
template <IsFieldsItem... Items>
struct Fields {};

}  // namespace mempeep