#pragma once

#include <Runtime/Detail/RuntimeServices.hpp>
#include <Runtime/RuntimeValue.hpp>

namespace ludork::runtime::detail {

RuntimeValue readRuntimeReference(const sol::object& value);
sol::object resolveRuntimeAttrValueType(sol::state_view lua,
                                        const sol::object& owner,
                                        const std::string& key);

void clearRuntimeCommonCaches(sol::state_view lua);

}  // namespace ludork::runtime::detail
