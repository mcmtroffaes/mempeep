#pragma once

#include <array>
#include <optional>
#include <type_traits>
#include <vector>

namespace mempeep::detail {

template <auto M>
  requires std::is_member_object_pointer_v<decltype(M)>
struct member_traits;

template <typename C, typename T, T C::* M>
struct member_traits<M> {
  using class_type = C;
  using member_type = T;
};

template <auto M>
using member_class_t = typename member_traits<M>::class_type;

template <auto M>
using member_type_t = typename member_traits<M>::member_type;

}  // namespace mempeep::detail