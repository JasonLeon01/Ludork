#pragma once

#include "Detail/CopyRuntime.hpp"

#include <LuaGlue/LuaGlue.hpp>

#include <optional>
#include <string>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

lua_glue::Object nilObject(lua_glue::StateView lua);
std::string popLuaError(lua_State* state, const char* fallback);
lua_glue::Object protectedIndex(lua_glue::StateView lua,
                                const lua_glue::Object& target,
                                const lua_glue::Object& key);
void protectedAssign(lua_glue::StateView lua, const lua_glue::Object& target,
                     const lua_glue::Object& key,
                     const lua_glue::Object& value);
bool tableHasMetatable(const lua_glue::Table& value);
lua_glue::Table createWeakTable(lua_glue::StateView lua, const char* mode);
lua_glue::Table registryTable(lua_glue::StateView lua, const char* key,
                              const char* weakMode = nullptr);
lua_glue::Object nativeDeepCopyProtocolsKey(lua_glue::StateView lua);
lua_glue::Table nativeDeepCopyProtocols(lua_glue::StateView lua);
std::optional<NativeDeepCopyProtocol> findNativeDeepCopyProtocol(
    lua_glue::StateView lua, const lua_glue::Object& nativeType);
bool tableIsEmpty(const lua_glue::Table& table);
bool rawBool(const lua_glue::Table& table, const char* name);
bool luaValuesEqual(lua_glue::StateView lua, const lua_glue::Object& left,
                    const lua_glue::Object& right);
bool objectsRawEqual(const lua_glue::Object& left,
                     const lua_glue::Object& right);

}  // namespace ludork::standard::class_runtime::detail
