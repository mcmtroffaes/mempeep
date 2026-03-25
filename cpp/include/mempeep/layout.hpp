#pragma once

#include <cstddef>                    // std::size_t
#include <mempeep/concepts.hpp>       // IsAddress
#include <mempeep/descriptor.hpp>     // Primitive, Ref, Array, Struct, ...
#include <mempeep/detail/traits.hpp>  // member_type_t, ...

namespace mempeep {

/**
 * @brief A field. Its type must be readable.
 *
 * For example, Field<&Class::member>.
 *
 * @tparam M The field to deserialize into.
 */
template <IsDescriptor Desc, auto M>
  requires std::same_as<typename Desc::native_type, detail::member_type_t<M>>
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

}  // namespace mempeep