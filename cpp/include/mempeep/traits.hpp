#pragma once

#include <optional>
#include <string_view>
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

template <auto M>
consteval std::string_view member_name() {
#if defined(_MSC_VER)
  constexpr std::string_view sig = __FUNCSIG__;
  constexpr auto last_colon = sig.rfind(':');
  constexpr auto close = sig.rfind('>');
#else
  constexpr std::string_view sig = __PRETTY_FUNCTION__;
  constexpr auto last_colon = sig.rfind(':');
  constexpr auto close = sig.rfind(']');
#endif
  static_assert(
    last_colon != std::string_view::npos,
    "member_name(): failed to find ':' in function signature"
  );
  static_assert(
    close != std::string_view::npos,
    "member_name(): failed to find closing delimiter in function signature"
  );
  static_assert(
    last_colon + 1 < close, "member_name(): invalid substring bounds"
  );
  constexpr auto len = close - last_colon - 1;
  static_assert(len > 0, "member_name(): extracted name is empty");
  return sig.substr(last_colon + 1, close - last_colon - 1);
}

}  // namespace mempeep