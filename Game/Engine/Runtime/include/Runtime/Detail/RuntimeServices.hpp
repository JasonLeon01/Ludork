#pragma once

#include <RuntimeApi.hpp>

#include <LuaGlue/LuaGlue.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ludork::runtime::detail {

LUDORK_RUNTIME_API void ensureRuntimeLuaStack(lua_State* state,
                                              std::size_t count,
                                              const char* context);
LUDORK_RUNTIME_API int invokeRuntimeFunction(
    lua_State* state, const lua_glue::Object& callable,
    const std::vector<lua_glue::Object>& arguments, const char* context);
LUDORK_RUNTIME_API lua_glue::Object nilObject(lua_glue::StateView lua);
LUDORK_RUNTIME_API lua_glue::Table createWeakTable(lua_glue::StateView lua,
                                                   const char* mode);
LUDORK_RUNTIME_API lua_glue::Table registryTable(
    lua_glue::StateView lua, const char* key, const char* weakMode = nullptr);
LUDORK_RUNTIME_API lua_glue::Table objectMetatable(
    lua_glue::StateView lua, const lua_glue::Object& value);
LUDORK_RUNTIME_API lua_glue::Object classType(lua_glue::ThisState state,
                                              const lua_glue::Object& value);
LUDORK_RUNTIME_API lua_glue::Object findRuntimeClassModule(
    lua_glue::StateView lua, const lua_glue::Object& classReference);
LUDORK_RUNTIME_API lua_glue::Object syntheticRuntimeMetadata(
    lua_glue::StateView lua, const lua_glue::Table& classTable);
LUDORK_RUNTIME_API lua_glue::Object runtimeTypeMetadata(
    lua_glue::StateView lua, const lua_glue::Table& classType);
LUDORK_RUNTIME_API lua_glue::Table collectRuntimeAttrMetadata(
    lua_glue::StateView lua, const lua_glue::Table& owner);
LUDORK_RUNTIME_API std::pair<lua_glue::Object, lua_glue::Object>
resolveRuntimeConfigVar(lua_glue::StateView lua, const lua_glue::Object& owner,
                        const lua_glue::Object& rawName);
LUDORK_RUNTIME_API std::pair<lua_glue::Object, lua_glue::Object>
resolveRuntimeMemberMetadata(lua_glue::StateView lua,
                             const lua_glue::Object& owner,
                             const lua_glue::Object& rawName);
LUDORK_RUNTIME_API lua_glue::Object evaluateRuntimeExpression(
    lua_glue::StateView lua, const lua_glue::Object& rawExpression,
    const lua_glue::Object& rawEnvironment);
LUDORK_RUNTIME_API lua_glue::Object resolveRuntimeMetadataType(
    lua_glue::StateView lua, const lua_glue::Object& typeReference,
    const lua_glue::Object& declaringModule);
LUDORK_RUNTIME_API bool runtimeSequence(const lua_glue::Table& table,
                                        std::vector<lua_glue::Object>& values);
LUDORK_RUNTIME_API lua_glue::Object runtimeIndex(lua_glue::StateView lua,
                                                 const lua_glue::Object& target,
                                                 const lua_glue::Object& key,
                                                 bool raw);
LUDORK_RUNTIME_API bool rawBool(const lua_glue::Table& table, const char* name);
LUDORK_RUNTIME_API bool isClass(const lua_glue::Table& value);
LUDORK_RUNTIME_API bool isNativeType(lua_glue::StateView lua,
                                     const lua_glue::Table& value);
LUDORK_RUNTIME_API bool isInstance(lua_glue::ThisState state,
                                   const lua_glue::Object& value,
                                   const lua_glue::Table& targetClass);
LUDORK_RUNTIME_API bool rawEqual(lua_glue::StateView lua,
                                 const lua_glue::Object& left,
                                 const lua_glue::Object& right);
LUDORK_RUNTIME_API lua_glue::Object checkedResult(lua_glue::StateView lua,
                                                  lua_glue::CallResult& result);
LUDORK_RUNTIME_API lua_glue::Table requireLuaTable(lua_glue::StateView lua,
                                                   const char* moduleName);
LUDORK_RUNTIME_API lua_glue::Object requireRuntimeType(
    lua_glue::StateView lua, const std::string& modulePath,
    const std::string& typeName);
LUDORK_RUNTIME_API bool luaBoolean(const lua_glue::Object& value);
LUDORK_RUNTIME_API std::vector<lua_glue::Table> runtimeClassMro(
    lua_glue::StateView lua, const lua_glue::Table& classTable);
LUDORK_RUNTIME_API void runtimeAssign(lua_glue::StateView lua,
                                      const lua_glue::Object& target,
                                      const lua_glue::Object& key,
                                      const lua_glue::Object& value, bool raw);
LUDORK_RUNTIME_API std::vector<lua_glue::Object> runtimeKeys(
    lua_glue::StateView lua, const lua_glue::Object& target, bool raw);

LUDORK_RUNTIME_API void clearRuntimeCaches(lua_glue::StateView lua);

}  // namespace ludork::runtime::detail
