#pragma once

#include <Runtime/Detail/RuntimeServices.hpp>

namespace ludork::runtime::detail {

sol::table runtimeClassTypeDescriptor(sol::state_view lua,
                                      const sol::table& classReference);

sol::object resolveRuntimeAttrValueType(sol::state_view lua,
                                        const sol::object& owner,
                                        const std::string& key);

}  // namespace ludork::runtime::detail
