#pragma once

#include <sol2/sol.hpp>

namespace ludork::standard::class_runtime::detail {

bool isNativeType(sol::state_view lua, const sol::table& value);
bool isCompositeInstance(sol::state_view lua, const sol::object& instance);
sol::object scriptClassOf(sol::state_view lua, const sol::object& value);
sol::object typeInfoOf(sol::state_view lua, const sol::table& nativeType);
sol::object nativeTypeOf(sol::state_view lua, const sol::object& value);
sol::object actualClassOf(sol::state_view lua, const sol::object& value);

}  // namespace ludork::standard::class_runtime::detail
