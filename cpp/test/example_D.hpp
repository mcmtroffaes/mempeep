#pragma once

#include <tuple>
#include <optional>

#include "mempeep/T.hpp"
#include "mempeep/D.hpp"

// Struct descriptors for structs in "example.hpp".

#pragma once

#include <mempeep/D.hpp>

#include "example.hpp"

using namespace mempeep;

template <> struct D::Struct<Entity> {
	using fields = std::tuple<
		D::Field<&Entity::pos>,
		D::Field<&Entity::health>
	>;
};

template <> struct D::Struct<State> {
	using fields = std::tuple<
		D::Field<&State::tick>,
		D::Field<&State::player>,
		D::Field<&State::enemy, T::Ptr<Entity> >
	>;
};
