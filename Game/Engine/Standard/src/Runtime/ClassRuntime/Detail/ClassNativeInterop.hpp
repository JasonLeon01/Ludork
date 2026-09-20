#pragma once

#include <LuaGlue/LuaGlue.hpp>

#include <cstddef>

namespace ludork::standard::class_native {

std::size_t nextInstanceId(lua_glue::StateView lua);
lua_glue::Table getUserFields(lua_glue::StateView lua,
                              const lua_glue::Object& value, bool create);
lua_glue::Table getObjectMetatable(lua_glue::StateView lua,
                                   const lua_glue::Object& value);

}  // namespace ludork::standard::class_native
