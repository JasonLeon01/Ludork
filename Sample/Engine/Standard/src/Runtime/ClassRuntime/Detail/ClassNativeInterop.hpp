#pragma once

#include <sol2/sol.hpp>

#include <cstddef>

namespace ludork::standard::class_native {

std::size_t nextInstanceId(sol::state_view lua);
sol::table getUserFields(sol::state_view lua, const sol::object& value,
                         bool create);
sol::table getObjectMetatable(sol::state_view lua, const sol::object& value);

}  // namespace ludork::standard::class_native
