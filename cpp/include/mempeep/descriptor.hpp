#pragma once

#include <cstddef>
#include <mempeep/layout.hpp>
#include <optional>

namespace mempeep::wip {

template <typename D>
concept IsDescriptor = requires { typename D::descriptor_tag; };

/**
 * @brief Reads sizeof(T) bytes directly into a T.
 */
template <typename T>
  requires IsPrimitive<T>
struct Primitive {
  using descriptor_tag = void;
  using value_type = T;
};

/**
 * @brief Reads a T using its remote_layout.
 */
template <HasRemoteLayout T>
struct RemoteLayout {
  using descriptor_tag = void;
  using value_type = T;
};

/**
 * @brief Reads sizeof(address_t) bytes.
 *
 * Stores the raw address as AddrT.
 */
template <IsAddress AddrT>
struct RawAddr {
  using descriptor_tag = void;
  using value_type = AddrT;
};

/**
 * @brief Reads an address, follows it, reads the pointee using Desc.
 *
 * Errors if null.
 */
template <IsDescriptor Desc>
struct Ref {
  using descriptor_tag = void;
  using value_type = typename Desc::value_type;
  using pointee_descriptor = Desc;
};

/**
 * @brief Like Ref, but null is allowed.
 *
 * Stored as `std::optional<...>`.
 */
template <IsDescriptor Desc>
struct NullableRef {
  using descriptor_tag = void;
  using value_type = std::optional<typename Desc::value_type>;
  using pointee_descriptor = Desc;
};

/**
 * @brief Array descriptor.
 *
 * Read N consecutive elements using Desc into `std::array<value_type, N>`.
 */
template <IsDescriptor Desc, std::size_t N>
struct Array {
  using descriptor_tag = void;
  using value_type = std::array<typename Desc::value_type, N>;
  using element_descriptor = Desc;
  static constexpr std::size_t count = N;
};

/**
 * @brief Reads a begin/end pointer pair, then each element using Desc,
 * into `std::vector<Desc::value_type>`.
 *
 * @tparam Desc   Descriptor for each element.
 *                `Desc::value_type` is the element type.
 * @tparam MaxLen Maximum number of elements to read before reporting
 *                `Error::VECTOR_TOO_LONG` and stopping.
 *                Prevents infinite loops on corrupt data.
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
 * Reads a head pointer from the current address (like NullableRef: null means
 * empty list), then follows next pointers until the address wraps back to the
 * head. Produces a `std::list<Desc::value_type>` containing the nodes in
 * traversal order.
 *
 * @tparam Desc   Descriptor for each node. `Desc::value_type` is the node type.
 * @tparam Next   Member pointer into `Desc::value_type` pointing at the field
 *                that holds the raw address of the next node.
 * @tparam MaxLen Maximum number of nodes to read before reporting
 *                `Error::CIRCULAR_LIST_TOO_LONG` and stopping.
 *                Prevents infinite loops on corrupt data.
 */
template <IsDescriptor Desc, auto Next, std::size_t MaxLen>
  requires IsAddress<member_type_t<Next>>
struct CircularList {
  using descriptor_tag = void;
  using value_type = std::vector<typename Desc::value_type>;
  using element_descriptor = Desc;
  static constexpr std::size_t max_len = MaxLen;
};

}  // namespace mempeep::wip