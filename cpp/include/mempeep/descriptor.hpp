// All descriptors.

#pragma once

#include <concepts>
#include <cstddef>
#include <mempeep/address.hpp>
#include <mempeep/detail/member_traits.hpp>
#include <optional>

namespace mempeep {

// --- Concepts for primitive and descriptor types. ---

/**
 * @brief Primitive types that be directly copied in memory.
 *
 * A type satisfies IsPrimitive if it can be copied byte-for-byte
 * (e.g. via memcpy) without requiring construction, destruction,
 * or pointer fixups.
 */
template <typename T>
concept IsPrimitive = std::is_trivially_copyable_v<T>;

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

/**
 * @brief Reads sizeof(T) bytes directly from remote memory into a T.
 *
 * @tparam T The primitive type to read. Must satisfy IsPrimitive.
 */
template <typename T>
  requires IsPrimitive<T>
struct Primitive {
  using native_type = T;
};

// --- Actual descriptors. ---

/**
 * @brief Reads an address without following it.
 *
 * Reads sizeof(reader's address type) bytes and stores the raw address
 * value as `AddrT`. No indirection is performed.
 *
 * @tparam AddrT The type to store the address in. Must satisfy IsAddress
 *               and be wide enough to hold the reader's address type.
 */
template <IsAddress AddrT>
struct RawAddr {
  using native_type = AddrT;
};

/**
 * @brief Reads an address and follows it, reading the pointee
 * using `Desc`.
 *
 * Reports `Error::ADDRESS_NULL` if the address is zero. The address
 * itself is not stored; only the pointee value is written into the native
 * object.
 *
 * @tparam Desc Descriptor for the pointee. `Desc::native_type` is the native
 *              type produced.
 */
template <IsDescriptor Desc>
struct Ref {
  using native_type = native_type_t<Desc>;
};

/**
 * @brief Like `Ref`, but a null address is allowed.
 *
 * If the address is zero, the field is set to `std::nullopt` and no error
 * is reported. If non-zero, the pointee is read using `Desc`. The address
 * word itself is not stored.
 *
 * @tparam Desc Descriptor for the pointee. The native type produced is
 *              `std::optional<native_type_t<Desc>>`.
 */
template <IsDescriptor Desc>
struct NullableRef {
  using native_type = std::optional<native_type_t<Desc>>;
};

/**
 * @brief Reads N consecutive elements from remote memory using Desc.
 *
 * Elements are read sequentially with no gaps between them. The cursor
 * advances by the total size of all elements.
 *
 * @tparam Desc Descriptor for each element. `native_type_t<Desc>` is the
 *              element type.
 * @tparam N    Number of elements to read.
 */
template <IsDescriptor Desc, std::size_t N>
struct Array {
  using native_type = std::array<native_type_t<Desc>, N>;
};

/**
 * @brief Reads a begin/end address pair, then each element using `Desc`.
 *
 * The two addresses are read from the current address. Elements are
 * then read from [begin, end). Reports `Error::ADDRESS_NULL` if begin is
 * zero, `Error::VECTOR_INVALID_RANGE` if begin is past end,
 * `Error::VECTOR_MISALIGNED` if end is not reachable by advancing by whole
 * element sizes, and `Error::VECTOR_TOO_LONG` if the element count exceeds
 * `MaxLen`. The cursor advances past the two addresses only, not past
 * the elements.
 *
 * @tparam Desc   Descriptor for each element. `Desc::native_type` is the
 *                element type.
 * @tparam MaxLen Maximum number of elements to read before reporting
 *                `Error::VECTOR_TOO_LONG` and stopping.
 */
template <IsDescriptor Desc, std::size_t MaxLen>
struct Vector {
  using native_type = std::vector<native_type_t<Desc>>;
};

/**
 * @brief Descriptor for a circular intrusive linked list.
 *
 * Reads a head address from the current address. If null, the list is
 * empty and no error is reported. Otherwise, nodes are read starting from
 * the head address using Desc, and traversal continues by following the
 * address stored in the `Next` field of each decoded node until that address
 * equals the head address. Reports `Error::CIRCULAR_LIST_TOO_LONG` if the
 * node count exceeds `MaxLen` before the list closes. The cursor advances
 * past the stored head address only, not past the nodes themselves.
 *
 * @tparam Desc   Descriptor for each node. Desc::native_type is the node
 *                type.
 * @tparam Next   Member pointer into Desc::native_type identifying the
 *                field that holds the raw address of the next node. Must
 *                satisfy IsAddress.
 * @tparam MaxLen Maximum number of nodes to read before reporting
 *                Error::CIRCULAR_LIST_TOO_LONG and stopping. Prevents
 *                infinite loops on corrupt data.
 */
template <IsDescriptor Desc, auto Next, std::size_t MaxLen>
  requires std::
             same_as<detail::member_class_t<Next>, native_type_t<Desc>>
           && IsAddress<detail::member_type_t<Next>>
struct CircularList {
  using native_type = std::vector<native_type_t<Desc>>;
};

// In the remainder of this file we set up everything for the Struct descriptor.
// The syntax is `Struct<T, Fields<Field<...>, Pad<...>, Seek<...>, ...>>`.
// So we need Field, Pad, Seek, Fields, and finally Struct.

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

// concept to aid type checking
template <typename T>
concept IsFieldsItem = requires { typename T::fields_item_tag; };

/**
 * @brief Sequence of field items.
 *
 * @tparam Items The field items, mapping the memory layout.
 */
template <IsFieldsItem... Items>
struct Fields {};

// Base template for packing the field items inside `Struct`.
template <typename T, typename FieldsT>
struct Struct;

/**
 * @brief Reads a struct using its own fields.
 *
 * @tparam T     The struct type to read.
 * @tparam Items The field items, mapping the memory layout.
 */
template <typename T, IsFieldsItem... Items>
struct Struct<T, Fields<Items...>> {
  using native_type = T;
};

}  // namespace mempeep