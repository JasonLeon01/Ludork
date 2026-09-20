#pragma once

#include <LuaGlue/LuaGlue.hpp>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

bool hasExplicitNilField(lua_State* state, int targetIndex, int keyIndex);
void clearExplicitNilField(lua_State* state, int targetIndex, int keyIndex);
bool hasExplicitNilField(lua_glue::StateView lua,
                         const lua_glue::Object& target,
                         const lua_glue::Object& key);
void clearExplicitNilField(lua_glue::StateView lua,
                           const lua_glue::Object& target,
                           const lua_glue::Object& key);
void markExplicitNilField(lua_glue::StateView lua,
                          const lua_glue::Object& target,
                          const lua_glue::Object& key);
lua_glue::Table explicitNilFieldKeys(lua_glue::StateView lua,
                                     const lua_glue::Object& target);
void copyExplicitNilFields(lua_glue::StateView lua,
                           const lua_glue::Object& source,
                           const lua_glue::Object& target);
void clearExplicitNilFields(lua_glue::StateView lua,
                            const lua_glue::Object& target);
void clearExplicitNilFields(lua_State* state);

}  // namespace ludork::standard::class_runtime::detail
