#pragma once

#include <cstddef>
#include <mempeep/layout.hpp>
#include <optional>

namespace mempeep::wip {

/**
 * @brief Concept satisfied by all descriptor types.
 *
 * A descriptor describes how to read a value from remote memory and what
 * native type it produces. Every descriptor exposes a `value_type` alias
 * giving the native type it populates.
 */
template <typename Desc>
concept IsDescriptor = requires {
  // extra tag to exclude non-mempeep types who expose value_type
  typename Desc::descriptor_tag;
  typename Desc::value_type;
};

/**
 * @brief Reads sizeof(T) bytes directly from remote memory into a T.
 *
 * @tparam T The primitive type to read. Must satisfy IsPrimitive.
 */
template <typename T>
  requires IsPrimitive<T>
struct Primitive {
  using descriptor_tag = void;
  using value_type = T;
};

/**
 * @brief Reads a struct using its own fields.
 *
 * Delegates to the layout machinery for T, allowing structs with a
 * `fields` to be used as descriptors and composed inside `Array`,
 * `Vector`, `Ref`, etc.
 *
 * @tparam T The struct type to read. Must satisfy IsStruct.
 */
template <IsStruct T>
struct Struct {
  using descriptor_tag = void;
  using value_type = T;
};

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
  using descriptor_tag = void;
  using value_type = AddrT;
};

/**
 * @brief Reads an address and follows it, reading the pointee
 * using `Desc`.
 *
 * Reports `Error::ADDRESS_NULL` if the address is zero. The address
 * itself is not stored; only the pointee value is written into the native
 * object.
 *
 * @tparam Desc Descriptor for the pointee. `Desc::value_type` is the native
 *              type produced.
 */
template <IsDescriptor Desc>
struct Ref {
  using descriptor_tag = void;
  using value_type = typename Desc::value_type;
  using element_descriptor = Desc;
};

/**
 * @brief Like `Ref`, but a null address is allowed.
 *
 * If the address is zero, the field is set to `std::nullopt` and no error
 * is reported. If non-zero, the pointee is read using `Desc`. The address
 * word itself is not stored.
 *
 * @tparam Desc Descriptor for the pointee. The native type produced is
 *              `std::optional<Desc::value_type>`.
 */
template <IsDescriptor Desc>
struct NullableRef {
  using descriptor_tag = void;
  using value_type = std::optional<typename Desc::value_type>;
  using element_descriptor = Desc;
};

/**
 * @brief Reads N consecutive elements from remote memory using Desc.
 *
 * Elements are read sequentially with no gaps between them. The cursor
 * advances by the total size of all elements.
 *
 * @tparam Desc Descriptor for each element. `Desc::value_type` is the
 *              element type.
 * @tparam N    Number of elements to read.
 */
template <IsDescriptor Desc, std::size_t N>
struct Array {
  using descriptor_tag = void;
  using value_type = std::array<typename Desc::value_type, N>;
  using element_descriptor = Desc;
  static constexpr std::size_t count = N;
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
 * @tparam Desc   Descriptor for each element. `Desc::value_type` is the
 *                element type.
 * @tparam MaxLen Maximum number of elements to read before reporting
 *                `Error::VECTOR_TOO_LONG` and stopping.
 */
template <IsDescriptor Desc, std::size_t MaxLen>
struct Vector {
  using descriptor_tag = void;
  using value_type = std::vector<typename Desc::value_type>;
  using element_descriptor = Desc;
  static constexpr std::size_t max_len = MaxLen;
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
 * @tparam Desc   Descriptor for each node. Desc::value_type is the node
 *                type.
 * @tparam Next   Member pointer into Desc::value_type identifying the
 *                field that holds the raw address of the next node. Must
 *                satisfy IsAddress.
 * @tparam MaxLen Maximum number of nodes to read before reporting
 *                Error::CIRCULAR_LIST_TOO_LONG and stopping. Prevents
 *                infinite loops on corrupt data.
 */
template <IsDescriptor Desc, auto Next, std::size_t MaxLen>
  requires std::same_as<member_class_t<Next>, typename Desc::value_type>
           && IsAddress<member_type_t<Next>>
struct CircularList {
  using descriptor_tag = void;
  using value_type = std::vector<typename Desc::value_type>;
  using element_descriptor = Desc;
  static constexpr std::size_t max_len = MaxLen;
};

}  // namespace mempeep::wip