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

template <typename T>
struct unwrap_optional {};

template <typename U>
struct unwrap_optional<std::optional<U>> {
  using type = U;
};

template <typename U>
using unwrap_optional_t = typename unwrap_optional<U>::type;

template <typename T>
struct unwrap_array {};

template <typename U, std::size_t N>
struct unwrap_array<std::array<U, N>> {
  using type = U;
  static constexpr std::size_t size = N;
};

template <typename T>
using unwrap_array_t = typename unwrap_array<T>::type;

template <typename T>
static constexpr std::size_t unwrap_array_size = unwrap_array<T>::size;

template <typename T>
struct unwrap_vector {};

template <typename U>
struct unwrap_vector<std::vector<U>> {
  using type = U;
};

template <typename T>
using unwrap_vector_t = typename unwrap_vector<T>::type;

}  // namespace mempeep::detail