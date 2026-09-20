#pragma once

#include <LuaGlue/LuaGlue.hpp>

namespace ludork::standard::class_runtime::detail {

bool isNativeType(lua_glue::StateView lua, const lua_glue::Table& value);
bool isCompositeInstance(lua_glue::StateView lua,
                         const lua_glue::Object& instance);
lua_glue::Object scriptClassOf(lua_glue::StateView lua,
                               const lua_glue::Object& value);
lua_glue::Object typeInfoOf(lua_glue::StateView lua,
                            const lua_glue::Table& nativeType);
lua_glue::Object nativeTypeOf(lua_glue::StateView lua,
                              const lua_glue::Object& value);
lua_glue::Object actualClassOf(lua_glue::StateView lua,
                               const lua_glue::Object& value);

}  // namespace ludork::standard::class_runtime::detail
