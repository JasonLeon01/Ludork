#pragma once

#include <Runtime/Detail/RuntimeServices.hpp>

namespace ludork::runtime::detail {

inline constexpr const char* CLASS_TYPE_METADATA_CACHE_KEY =
    "Ludork.Runtime.classTypeMetadataCache";

sol::object resolveRuntimeAttrValueType(sol::state_view lua,
                                        const sol::object& owner,
                                        const std::string& key);

}  // namespace ludork::runtime::detail
