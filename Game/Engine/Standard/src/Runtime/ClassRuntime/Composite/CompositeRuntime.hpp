#pragma once

#include "Detail/RuntimeState.hpp"

#include <LuaGlue/LuaGlue.hpp>

#include <string>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

lua_glue::Object compositeIndexSlow(lua_glue::Object target,
                                    lua_glue::Object key,
                                    lua_glue::ThisState state);
void invalidateFastIndexEntry(lua_State* state, int cacheIndex);
int compositeIndex(lua_State* state);
int compositeNewIndex(lua_State* state);
lua_glue::Table compositeMetatable(lua_glue::StateView lua);
lua_glue::Table constructingCompositeMetatable(lua_glue::StateView lua);
void invokeMonitorCallbacks(lua_glue::StateView lua,
                            const lua_glue::Table& entry,
                            const lua_glue::Object& oldValue,
                            const lua_glue::Object& newValue);
void registerMonitor(lua_glue::ThisState state, const lua_glue::Object& target,
                     const std::string& name,
                     const lua_glue::Function& callback,
                     lua_glue::Arguments options);
void unregisterMonitor(lua_glue::ThisState state,
                       const lua_glue::Object& target, const std::string& name,
                       std::optional<std::string> identifier);

}  // namespace ludork::standard::class_runtime::detail
