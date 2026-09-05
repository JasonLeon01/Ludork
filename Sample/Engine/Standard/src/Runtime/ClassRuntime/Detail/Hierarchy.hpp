#pragma once

#include "Detail/RuntimeState.hpp"

#include <sol2/sol.hpp>

#include <cstddef>
#include <vector>

namespace ludork::standard::class_runtime::detail {

bool isClass(const sol::table& value);
sol::table classLookupOwners(sol::state_view lua, sol::table classTable,
                             const char* category);
void invalidateClassLookup(sol::state_view lua, sol::table classTable);
void registerSubclass(sol::state_view lua, sol::table base,
                      const sol::table& subclass);
std::vector<sol::table> tableList(const sol::table& values);
void ensureMroSet(sol::state_view lua, sol::table type, const sol::table& mro,
                  const char* setName);
std::vector<sol::table> createMro(const sol::table& type,
                                  const sol::table& bases, MroKind kind);
sol::table getMro(sol::state_view lua, sol::table type);
sol::table getNativeMro(sol::state_view lua, sol::table type);
sol::table getBases(sol::state_view lua, const sol::table& classTable);
sol::object rawMember(sol::state_view lua, const sol::table& type,
                      const sol::object& key);
sol::object findInClass(sol::state_view lua, const sol::table& classTable,
                        const sol::object& key, bool includeClass = true);
sol::object findAccessor(sol::state_view lua, const sol::table& classTable,
                         const char* collectionName, const sol::object& key);
sol::object findScriptMember(sol::state_view lua, const sol::table& classTable,
                             const sol::object& key);
sol::object findClassOverride(sol::state_view lua, const sol::table& classTable,
                              const sol::object& key);
bool derivesFrom(sol::state_view lua, const sol::table& classTable,
                 const sol::table& targetClass);
sol::object scriptClassOf(sol::state_view lua, const sol::object& value);
sol::object typeInfoOf(sol::state_view lua, const sol::table& nativeType);
sol::object nativeTypeOf(sol::state_view lua, const sol::object& value);
sol::object actualClassOf(sol::state_view lua, const sol::object& value);

}  // namespace ludork::standard::class_runtime::detail
