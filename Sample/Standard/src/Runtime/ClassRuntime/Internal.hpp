#pragma once

#include "ClassRuntime.hpp"
#include "ClassNativeInterop.hpp"
#include "ClassRuntimeInternal.hpp"

#include <ClassServices.hpp>
#include <LuaError.hpp>
#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ludork::standard::class_runtime::detail {

// ── Registry key constants ───────────────────────────────────────────────────
inline constexpr const char* METHOD_OWNERS_KEY = "Ludork.Class.methodOwners";
inline constexpr const char* NATIVE_TYPE_CACHE_KEY = "Ludork.Class.nativeTypeCache";
inline constexpr const char* INSTANCES_KEY = "Ludork.Class.instances";
inline constexpr const char* COMPOSITE_METATABLE_KEY =
    "Ludork.Class.compositeMetatable";
inline constexpr const char* CONSTRUCTING_COMPOSITE_METATABLE_KEY =
    "Ludork.Class.constructingCompositeMetatable";
inline constexpr const char* NATIVE_OWNERS_KEY = "Ludork.Class.nativeOwners";
inline constexpr const char* NATIVE_POINTER_OWNERS_KEY =
    "Ludork.Class.nativePointerOwners";
inline constexpr const char* DYNAMIC_NATIVE_WRITERS_KEY =
    "Ludork.Class.dynamicNativeWriters";
inline constexpr const char* SUPER_PROXY_CACHE_KEY = "Ludork.Class.superProxyCache";
inline constexpr const char* SUPER_PROXY_METATABLE_KEY =
    "Ludork.Class.superProxyMetatable";
inline constexpr const char* MONITOR_STATES_KEY = "Ludork.Class.monitorStates";
inline constexpr const char* LIFECYCLE_STATES_KEY = "Ludork.Class.lifecycleStates";
inline constexpr const char* DISPOSED_METATABLE_KEY =
    "Ludork.Class.disposedMetatable";
inline constexpr const char* SHUTTING_DOWN_KEY = "Ludork.Class.shuttingDown";

// ── Field name constants ─────────────────────────────────────────────────────
inline constexpr const char* NATIVE_METHOD_CACHE_FIELD = "__nativeMethodCache";
inline constexpr const char* FAST_INDEX_CACHE_FIELD = "__fastIndexCache";
inline constexpr const char* NATIVE_INITIALIZER_FIELD = "__classInit";
inline constexpr const char* NATIVE_INITIALIZING_FIELD = "__initializing";
inline constexpr const char* NATIVE_CONSTRUCTION_FAILED_FIELD =
    "__constructionFailed";
inline constexpr const char* NATIVE_CONSTRUCTING_ROOTS_FIELD =
    "__nativeConstructingRoots";
inline constexpr const char* NATIVE_DIRTY_PROPERTIES_FIELD =
    "__nativeDirtyProperties";
inline constexpr const char* NATIVE_CLASS_INDEX_FIELD =
    "__ludorkNativeClassIndex";
inline constexpr const char* NATIVE_CLASS_NEW_INDEX_FIELD =
    "__ludorkNativeClassNewIndex";
inline constexpr const char* NATIVE_CLASS_GUARD_FIELD =
    "__ludorkNativeClassGuard";
inline constexpr const char* NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD =
    "__classResolvedDefaults";
inline constexpr const char* NATIVE_COPY_FIELD = "__copy";

// ── Lightuserdata identity keys (defined once in Common.cpp) ─────────────────
extern unsigned char nativeClassDefaultResolverKeyStorage;
extern unsigned char nativeDeepCopyProtocolsKeyStorage;

// ── Enumerations ─────────────────────────────────────────────────────────────
enum class LifecycleState : lua_Integer {
    Active = 0,
    Disposing = 1,
    Disposed = 2,
};

enum class FastIndexKind : lua_Integer {
    Value = 1,
    ScriptMember = 2,
    Getter = 3,
    NativeMember = 4,
};

enum class MroKind {
    Runtime,
    Native
};

// ── Type helpers ─────────────────────────────────────────────────────────────
using NativeShadowSnapshot = std::vector<std::pair<sol::object, sol::object>>;

// ── Common.cpp ───────────────────────────────────────────────────────────────
sol::object nilObject(sol::state_view lua);
std::string popLuaError(lua_State* state, const char* fallback);
sol::object protectedIndex(sol::state_view lua, const sol::object& target,
                           const sol::object& key);
void protectedAssign(sol::state_view lua, const sol::object& target,
                     const sol::object& key, const sol::object& value);
bool isClass(const sol::table& value);
bool tableHasMetatable(const sol::table& value);
sol::table createWeakTable(sol::state_view lua, const char* mode);
sol::table registryTable(sol::state_view lua, const char* key,
                         const char* weakMode = nullptr);
sol::object nativeDeepCopyProtocolsKey(sol::state_view lua);
sol::table nativeDeepCopyProtocols(sol::state_view lua);
std::optional<NativeDeepCopyProtocol> findNativeDeepCopyProtocol(
    sol::state_view lua, const sol::object& nativeType);
bool tableIsEmpty(const sol::table& table);
bool rawBool(const sol::table& table, const char* name);
bool luaValuesEqual(sol::state_view lua, const sol::object& left,
                    const sol::object& right);
sol::table classLookupOwners(sol::state_view lua, sol::table classTable,
                             const char* category);
void invalidateClassLookup(sol::state_view lua, sol::table classTable);
void registerSubclass(sol::state_view lua, sol::table base,
                      const sol::table& subclass);
std::vector<sol::table> tableList(const sol::table& values);
bool containsTable(const std::vector<sol::table>& values, std::size_t start,
                   const sol::table& target);
sol::table getMro(sol::state_view lua, sol::table type);
sol::table getNativeMro(sol::state_view lua, sol::table type);
void ensureMroSet(sol::state_view lua, sol::table type, const sol::table& mro,
                  const char* setName);
std::vector<sol::table> createMro(const sol::table& type,
                                  const sol::table& bases, MroKind kind);
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

// ── Native.cpp ───────────────────────────────────────────────────────────────
bool objectsRawEqual(const sol::object& left, const sol::object& right);
bool isNativeInitializer(const sol::table& nativeType, const sol::object& member);
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
bool nativeTypeAccepts(sol::state_view lua, const sol::table& nativeType,
                       const sol::object& value);
sol::object nativeClassDefaultResolverKey(sol::state_view lua);

// ── Composite.cpp ────────────────────────────────────────────────────────────
sol::object compositeIndexSlow(sol::object target, sol::object key,
                               sol::this_state state);
void invalidateFastIndexEntry(lua_State* state, int cacheIndex);
int compositeIndex(lua_State* state);
int compositeNewIndex(lua_State* state);
sol::table compositeMetatable(sol::state_view lua);
sol::table constructingCompositeMetatable(sol::state_view lua);
bool isCompositeInstance(sol::state_view lua, const sol::object& instance);
void markNativePropertyDirty(sol::state_view lua, sol::table fields,
                             const sol::object& nativeObject,
                             const sol::object& key);
void syncNativeRootDefaults(sol::state_view lua, const sol::table& classTable,
                            const sol::object& instance, const sol::table& root,
                            const sol::object& nativeObject,
                            NativeShadowSnapshot& shadowSnapshot);
void replayNativeDirtyProperties(sol::state_view lua, const sol::table& fields,
                                 const sol::table& root,
                                 const sol::object& source,
                                 const sol::object& destination);
void syncNativeClassDefaults(sol::state_view lua, const sol::table& classTable,
                             const sol::object& instance);
void restoreNativeShadows(sol::table fields, const NativeShadowSnapshot& snapshot);
void registerMonitor(sol::this_state state, const sol::object& target,
                     const std::string& name,
                     const sol::protected_function& callback,
                     sol::optional<sol::table> params);
void unregisterMonitor(sol::this_state state, const sol::object& target,
                       const std::string& name);

// ── Instance.cpp ─────────────────────────────────────────────────────────────
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
void reportDisposeError(const char* phase, const std::string& message);
bool nativeRootIsDeferred(const sol::table& root);
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

// ── Copy.cpp ─────────────────────────────────────────────────────────────────
bool isAtomicClassTable(sol::state_view lua, const sol::table& value);
sol::object copyNativeValue(sol::state_view lua, const sol::object& value);
sol::object shallowCopyImpl(sol::state_view lua, const sol::object& value);
sol::object deepCopyImpl(sol::state_view lua, const sol::object& value,
                         std::unordered_map<const void*, sol::object>& visited);
sol::object deepCopyImpl(sol::state_view lua, const sol::object& value);

// ── Module.cpp ───────────────────────────────────────────────────────────────
struct CallableInfo {
    int parameterCount = 0;
    bool vararg = false;
    std::vector<std::string> parameterNames;
};

CallableInfo inspectCallable(const sol::object& callable);
sol::table constructorClass(lua_State* state);
void setClassClosure(lua_State* state, const sol::table& target,
                     const char* name, const sol::table& classTable,
                     lua_CFunction function);
sol::table finalizeClassImpl(sol::table definition, const sol::table& bases);
sol::table ownFields(sol::state_view lua, const sol::object& target);
sol::object rawOwnField(sol::state_view lua, const sol::object& target,
                        const sol::object& key);
bool hasRawOwnField(sol::state_view lua, const sol::object& target,
                    const sol::object& key);
sol::table ownKeyList(sol::state_view lua, const sol::object& target);
sol::table mroCopy(sol::state_view lua, const sol::object& value);
int classNew(lua_State* state);
int classCall(lua_State* state);

}  // namespace ludork::standard::class_runtime::detail
