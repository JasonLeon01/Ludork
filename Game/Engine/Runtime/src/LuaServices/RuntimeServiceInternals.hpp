#pragma once

#include <Runtime/Detail/RuntimeServices.hpp>

namespace ludork::runtime::detail {

lua_glue::Table runtimeClassTypeDescriptor(
    lua_glue::StateView lua, const lua_glue::Table& classReference);

lua_glue::Object resolveRuntimeAttrValueType(lua_glue::StateView lua,
                                             const lua_glue::Object& owner,
                                             const std::string& key);

}  // namespace ludork::runtime::detail
