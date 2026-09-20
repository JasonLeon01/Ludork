#pragma once

#include <StandardApi.hpp>

#include <LuaGlue/LuaGlue.hpp>

#include <string>

struct lua_State;

namespace ludork::standard::class_runtime {

LUDORK_STANDARD_API lua_glue::Table finalizeClass(lua_glue::Table definition,
                                                  const lua_glue::Table& bases);

LUDORK_STANDARD_API void registerNativeClass(lua_glue::Table nativeType,
                                             const lua_glue::Table& metadata);

LUDORK_STANDARD_API void registerNativeClassDefaultResolver(
    lua_glue::StateView lua, const lua_glue::Function& callback);

LUDORK_STANDARD_API void unregisterNativeClassDefaultResolver(
    lua_glue::StateView lua);

LUDORK_STANDARD_API lua_glue::Object protectedGet(
    lua_glue::StateView lua, const lua_glue::Object& target,
    const lua_glue::Object& key);

LUDORK_STANDARD_API void protectedSet(lua_glue::StateView lua,
                                      const lua_glue::Object& target,
                                      const lua_glue::Object& key,
                                      const lua_glue::Object& value);

LUDORK_STANDARD_API void protectedSetTyped(lua_glue::StateView lua,
                                           const lua_glue::Object& target,
                                           const lua_glue::Object& key,
                                           const lua_glue::Object& value);

LUDORK_STANDARD_API lua_glue::Object rawGetOwnField(
    lua_glue::StateView lua, const lua_glue::Object& target,
    const lua_glue::Object& key);

LUDORK_STANDARD_API bool hasOwnField(lua_glue::StateView lua,
                                     const lua_glue::Object& target,
                                     const lua_glue::Object& key);

LUDORK_STANDARD_API lua_glue::Table getOwnKeys(lua_glue::StateView lua,
                                               const lua_glue::Object& target);

LUDORK_STANDARD_API bool rawEqual(const lua_glue::Object& left,
                                  const lua_glue::Object& right);

LUDORK_STANDARD_API lua_glue::Table getMroCopy(lua_glue::StateView lua,
                                               const lua_glue::Object& value);

LUDORK_STANDARD_API lua_glue::Object typeOf(lua_glue::StateView lua,
                                            const lua_glue::Object& value);

LUDORK_STANDARD_API bool isInstanceOf(lua_glue::StateView lua,
                                      const lua_glue::Object& value,
                                      const lua_glue::Table& targetClass);

LUDORK_STANDARD_API bool isSubclassOf(lua_glue::StateView lua,
                                      const lua_glue::Table& value,
                                      const lua_glue::Table& targetClass);

LUDORK_STANDARD_API lua_glue::Object clonePlainData(
    lua_glue::StateView lua, const lua_glue::Object& value);

LUDORK_STANDARD_API lua_glue::Object shallowCopy(lua_glue::StateView lua,
                                                 const lua_glue::Object& value);

LUDORK_STANDARD_API lua_glue::Object deepCopy(lua_glue::StateView lua,
                                              const lua_glue::Object& value);

LUDORK_STANDARD_API lua_glue::Object requireModule(
    lua_glue::StateView lua, const std::string& moduleName);

LUDORK_STANDARD_API int invoke(lua_State* state,
                               const lua_glue::Object& callable,
                               int argumentCount);

}  // namespace ludork::standard::class_runtime
