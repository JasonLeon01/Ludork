#pragma once

#include <LuaGlue/LuaGlue.hpp>

#include <optional>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

lua_glue::Object ensureDefaultNativeObject(lua_glue::StateView lua,
                                           const lua_glue::Object& instance,
                                           const lua_glue::Table& nativeType);
int classInstanceGc(lua_State* state);
lua_glue::Object instanceDisposeMethod(lua_glue::StateView lua,
                                       const lua_glue::Table& classTable,
                                       const lua_glue::Object& key);
bool disposeInstanceCore(lua_glue::StateView lua,
                         const lua_glue::Object& instance, bool invokeDispose);
std::optional<lua_glue::Table> tryManagedInstanceFields(
    lua_glue::StateView lua, const lua_glue::Object& instance);
lua_glue::Table managedInstanceFields(lua_glue::StateView lua,
                                      const lua_glue::Object& instance);
bool nativeRootIsDeferred(const lua_glue::Table& root);
void unregisterNativeOwner(lua_glue::StateView lua,
                           const lua_glue::Object& nativeObject,
                           const lua_glue::Object& owner);
void clearNativeMethodCaches(lua_glue::StateView lua,
                             const lua_glue::Object& instance,
                             lua_glue::Table fields);
void ensureNativeInitializer(lua_glue::StateView lua,
                             lua_glue::Table nativeType);
void failNativeConstruction(lua_glue::StateView lua,
                            const lua_glue::Table& classTable,
                            const lua_glue::Object& instance);
bool compositeBelongsToClass(lua_glue::StateView lua,
                             const lua_glue::Object& instance,
                             const lua_glue::Table& classTable);
lua_glue::Object constructNativeRoot(lua_glue::StateView lua,
                                     const lua_glue::Table& classTable,
                                     const lua_glue::Object& instance,
                                     const lua_glue::Table& root,
                                     const lua_glue::Object& arguments);
void validateNativeRoots(lua_glue::StateView lua,
                         const lua_glue::Table& classTable,
                         const lua_glue::Object& instance);
void validateNativeInstanceShape(lua_glue::StateView lua,
                                 const lua_glue::Table& classTable,
                                 const lua_glue::Object& instance);
void completeDefaultNativeRoots(lua_glue::StateView lua,
                                const lua_glue::Table& classTable,
                                const lua_glue::Object& instance);

}  // namespace ludork::standard::class_runtime::detail
