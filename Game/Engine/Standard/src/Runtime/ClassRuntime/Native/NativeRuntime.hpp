#pragma once

#include "Detail/RuntimeState.hpp"

#include <LuaGlue/LuaGlue.hpp>

#include <string>
#include <vector>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

lua_glue::Object rawMember(lua_glue::StateView lua, const lua_glue::Table& type,
                           const lua_glue::Object& key);
lua_glue::Object findInClass(lua_glue::StateView lua,
                             const lua_glue::Table& classTable,
                             const lua_glue::Object& key,
                             bool includeClass = true);
bool nativeTypeAccepts(lua_glue::StateView lua,
                       const lua_glue::Table& nativeType,
                       const lua_glue::Object& value);
void registerMethodOwner(lua_glue::StateView lua,
                         const lua_glue::Table& classTable,
                         const lua_glue::Object& value);
void registerNativePointerOwner(lua_glue::StateView lua,
                                const lua_glue::Object& nativeObject,
                                const lua_glue::Object& owner);
void unregisterNativePointerOwner(lua_glue::StateView lua,
                                  const lua_glue::Object& nativeObject,
                                  const lua_glue::Object& owner);
bool pushNativeOwner(lua_State* state, int nativeIndex);
void restoreNativeOwners(lua_State* state);
void registerNativeInterop(lua_State* state);
lua_glue::Object bindMethod(lua_glue::StateView lua,
                            const lua_glue::Object& method,
                            const lua_glue::Object& self);
lua_glue::Object wrapNativeMethod(lua_glue::StateView lua,
                                  const lua_glue::Object& method,
                                  const lua_glue::Object& nativeObject);
bool isNativeInitializer(const lua_glue::Table& nativeType,
                         const lua_glue::Object& member);
std::string nativeTypeName(lua_glue::StateView lua,
                           const lua_glue::Table& nativeType);
lua_glue::Object nativeTypeDefinition(lua_glue::StateView lua,
                                      const lua_glue::Table& nativeType,
                                      const lua_glue::Object& key);
bool nativeTypeDeclaresProperty(const lua_glue::Table& nativeType,
                                const lua_glue::Object& key);
bool nativeClassProperty(lua_glue::StateView lua,
                         const lua_glue::Table& nativeType,
                         const lua_glue::Object& key, lua_glue::Object& value);
bool nativeFallbackMemberEligible(const lua_glue::Object& key);
std::vector<lua_glue::Table> nativeRoots(lua_glue::StateView lua,
                                         const lua_glue::Table& classTable);
lua_glue::Object nativeObjectForType(lua_glue::StateView lua,
                                     const lua_glue::Table& fields,
                                     const lua_glue::Table& nativeType);
lua_glue::Object cachedNativeMethod(lua_glue::StateView lua,
                                    lua_glue::Table fields,
                                    const lua_glue::Object& key,
                                    const lua_glue::Object& method,
                                    const lua_glue::Object& nativeObject,
                                    const lua_glue::Table& nativeType,
                                    bool objectMember);
lua_glue::Object findCachedNativeMethod(lua_glue::StateView lua,
                                        lua_glue::Table fields,
                                        const lua_glue::Object& key);
lua_Integer classLookupVersion(const lua_glue::Table& classTable);
lua_glue::Table fastIndexCache(lua_glue::StateView lua, lua_glue::Table fields);
void cacheFastIndex(lua_glue::StateView lua, lua_glue::Table fields,
                    const lua_glue::Table& classTable,
                    const lua_glue::Object& key, FastIndexKind kind,
                    const lua_glue::Object& route);
void cacheFastClassOwner(lua_glue::StateView lua, lua_glue::Table fields,
                         const lua_glue::Table& classTable,
                         const lua_glue::Object& key, const char* category,
                         FastIndexKind kind);

bool setNativeMember(lua_glue::StateView lua, const lua_glue::Table& fields,
                     const lua_glue::Table& classTable,
                     const lua_glue::Object& key, const lua_glue::Object& value,
                     lua_glue::Object* assignedObject = nullptr);
void markNativePropertyDirty(lua_glue::StateView lua, lua_glue::Table fields,
                             const lua_glue::Object& nativeObject,
                             const lua_glue::Object& key);
void syncNativeRootDefaults(lua_glue::StateView lua,
                            const lua_glue::Table& classTable,
                            const lua_glue::Object& instance,
                            const lua_glue::Table& root,
                            const lua_glue::Object& nativeObject,
                            NativeShadowSnapshot& shadowSnapshot);
void replayNativeDirtyProperties(lua_glue::StateView lua,
                                 const lua_glue::Table& fields,
                                 const lua_glue::Table& root,
                                 const lua_glue::Object& source,
                                 const lua_glue::Object& destination);
void syncNativeClassDefaults(lua_glue::StateView lua,
                             const lua_glue::Table& classTable,
                             const lua_glue::Object& instance);
void restoreNativeShadows(lua_glue::Table fields,
                          const NativeShadowSnapshot& snapshot);

}  // namespace ludork::standard::class_runtime::detail
