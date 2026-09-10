#pragma once

#include <Runtime/Detail/RuntimeServices.hpp>

namespace ludork::runtime::detail {

sol::object resolveRuntimeAttrValueType(sol::state_view lua,
                                        const sol::object& owner,
                                        const std::string& key);

}  // namespace ludork::runtime::detail
