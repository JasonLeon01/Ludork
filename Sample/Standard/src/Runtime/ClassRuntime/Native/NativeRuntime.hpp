#pragma once

#include "Detail/RuntimeState.hpp"

#include <sol2/sol.hpp>

#include <string>
#include <vector>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

bool nativeTypeAccepts(sol::state_view lua, const sol::table& nativeType,
                       const sol::object& value);
void registerMethodOwner(sol::state_view lua, const sol::table& classTable,
                         const sol::object& value);
void registerNativePointerOwner(sol::state_view lua,
                                const sol::object& nativeObject,
                                const sol::object& owner);
void unregisterNativePointerOwner(sol::state_view lua,
                                  const sol::object& nativeObject,
                                  const sol::object& owner);
bool pushNativeOwner(lua_State* state, int nativeIndex);
void restoreNativeOwners(lua_State* state);
sol::object bindMethod(sol::state_view lua, const sol::object& method,
                       const sol::object& self);
sol::object wrapNativeMethod(sol::state_view lua, const sol::object& method,
                             const sol::object& nativeObject);
int superFunction(lua_State* state);
bool isNativeInitializer(const sol::table& nativeType,
                         const sol::object& member);
bool isNativeType(sol::state_view lua, const sol::table& value);
std::string nativeTypeName(sol::state_view lua, const sol::table& nativeType);
sol::object nativeTypeDefinition(sol::state_view lua,
                                 const sol::table& nativeType,
                                 const sol::object& key);
bool nativeTypeDeclaresProperty(const sol::table& nativeType,
                                const sol::object& key);
bool nativeClassProperty(sol::state_view lua, const sol::table& nativeType,
                         const sol::object& key, sol::object& value);
bool nativeFallbackMemberEligible(const sol::object& key);
std::vector<sol::table> nativeRoots(sol::state_view lua,
                                    const sol::table& classTable);
sol::object nativeObjectForType(sol::state_view lua, const sol::table& fields,
                                const sol::table& nativeType);
sol::object cachedNativeMethod(sol::state_view lua, sol::table fields,
                               const sol::object& key,
                               const sol::object& method,
                               const sol::object& nativeObject,
                               const sol::table& nativeType, bool objectMember);
sol::object findCachedNativeMethod(sol::state_view lua, sol::table fields,
                                   const sol::object& key);
lua_Integer classLookupVersion(const sol::table& classTable);
sol::table fastIndexCache(sol::state_view lua, sol::table fields);
void cacheFastIndex(sol::state_view lua, sol::table fields,
                    const sol::table& classTable, const sol::object& key,
                    FastIndexKind kind, const sol::object& route);
void cacheFastClassOwner(sol::state_view lua, sol::table fields,
                         const sol::table& classTable, const sol::object& key,
                         const char* category, FastIndexKind kind);

}  // namespace ludork::standard::class_runtime::detail
