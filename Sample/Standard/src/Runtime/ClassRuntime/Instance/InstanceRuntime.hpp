#pragma once

#include <sol2/sol.hpp>

#include <optional>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

sol::object ensureDefaultNativeObject(sol::state_view lua,
                                      const sol::object& instance,
                                      const sol::table& nativeType);
int classInstanceGc(lua_State* state);
bool disposeInstanceCore(sol::state_view lua, const sol::object& instance,
                         bool invokeDispose);
sol::object allocateInstance(
    sol::state_view lua, const sol::table& classTable,
    const sol::object& constructorArguments = sol::object(),
    bool allowDeferredRoots = false);
std::optional<sol::table> tryManagedInstanceFields(sol::state_view lua,
                                                   const sol::object& instance);
sol::table managedInstanceFields(sol::state_view lua,
                                 const sol::object& instance);
bool nativeRootIsDeferred(const sol::table& root);
void unregisterNativeOwner(sol::state_view lua, const sol::object& nativeObject,
                           const sol::object& owner);
void clearNativeMethodCaches(sol::state_view lua, const sol::object& instance,
                             sol::table fields);
void ensureNativeInitializer(sol::state_view lua, sol::table nativeType);
void failNativeConstruction(sol::state_view lua, const sol::table& classTable,
                            const sol::object& instance);
void finishNativeConstruction(sol::state_view lua, const sol::table& classTable,
                              const sol::object& instance);
bool compositeBelongsToClass(sol::state_view lua, const sol::object& instance,
                             const sol::table& classTable);
sol::object constructNativeRoot(sol::state_view lua,
                                const sol::table& classTable,
                                const sol::object& instance,
                                const sol::table& root,
                                const sol::object& arguments);
void validateNativeRoots(sol::state_view lua, const sol::table& classTable,
                         const sol::object& instance);
void validateNativeInstanceShape(sol::state_view lua,
                                 const sol::table& classTable,
                                 const sol::object& instance);
void completeDefaultNativeRoots(sol::state_view lua,
                                const sol::table& classTable,
                                const sol::object& instance);

}  // namespace ludork::standard::class_runtime::detail
