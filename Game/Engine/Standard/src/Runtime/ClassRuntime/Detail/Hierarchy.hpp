#pragma once

#include "Detail/RuntimeState.hpp"

#include <LuaGlue/LuaGlue.hpp>

#include <cstddef>
#include <vector>

namespace ludork::standard::class_runtime::detail {

bool isClass(const lua_glue::Table& value);
lua_glue::Table classLookupOwners(lua_glue::StateView lua,
                                  lua_glue::Table classTable,
                                  const char* category);
void invalidateClassLookup(lua_glue::StateView lua, lua_glue::Table classTable);
void registerSubclass(lua_glue::StateView lua, lua_glue::Table base,
                      const lua_glue::Table& subclass);
std::vector<lua_glue::Table> tableList(const lua_glue::Table& values);
void ensureMroSet(lua_glue::StateView lua, lua_glue::Table type,
                  const lua_glue::Table& mro, const char* setName);
std::vector<lua_glue::Table> createMro(const lua_glue::Table& type,
                                       const lua_glue::Table& bases,
                                       MroKind kind);
lua_glue::Table getMro(lua_glue::StateView lua, lua_glue::Table type);
lua_glue::Table getNativeMro(lua_glue::StateView lua, lua_glue::Table type);
lua_glue::Table getBases(lua_glue::StateView lua,
                         const lua_glue::Table& classTable);
lua_glue::Object findAccessor(lua_glue::StateView lua,
                              const lua_glue::Table& classTable,
                              const char* collectionName,
                              const lua_glue::Object& key);
lua_glue::Object findScriptMember(lua_glue::StateView lua,
                                  const lua_glue::Table& classTable,
                                  const lua_glue::Object& key,
                                  bool* found = nullptr);
lua_glue::Object findClassOverride(lua_glue::StateView lua,
                                   const lua_glue::Table& classTable,
                                   const lua_glue::Object& key);
bool derivesFrom(lua_glue::StateView lua, const lua_glue::Table& classTable,
                 const lua_glue::Table& targetClass);
}  // namespace ludork::standard::class_runtime::detail
