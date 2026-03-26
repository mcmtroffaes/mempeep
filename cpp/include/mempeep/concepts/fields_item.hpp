#pragma once

namespace mempeep {

// concept to aid type checking
template <typename T>
concept IsFieldsItem = requires { typename T::fields_item_tag; };

}  // namespace mempeep