#pragma once

#include <optional>
#include <type_traits>

namespace mempeep {

template <auto M>
  requires std::is_member_object_pointer_v<decltype(M)>
struct member_type;

template <typename C, typename T, T C::* M>
struct member_type<M> {
  using type = T;
};

template <auto M>
using member_type_t = typename member_type<M>::type;

template <typename T>
struct unwrap_optional {};

template <typename U>
struct unwrap_optional<std::optional<U>> {
  using type = U;
};

template <typename T>
using unwrap_optional_t = typename unwrap_optional<T>::type;

}  // namespace mempeep