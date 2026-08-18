#include "Internal.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ludork::standard::class_runtime::detail {

// ── Native callback support ───────────────────────────────────────────────────

namespace {

bool pushNativeCallbackInstance(lua_State* state) {
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_pushnil(state);
    if (lua_next(state, -2) == 0) {
        lua_pop(state, 1);
        lua_pushnil(state);
        return false;
    }
    lua_pop(state, 1);
    lua_remove(state, -2);
    return true;
}

int nativeCallback(lua_State* state) {
    const int argumentCount = lua_gettop(state);
    const char* methodName = lua_tostring(state, lua_upvalueindex(2));
    if (!pushNativeCallbackInstance(state)) {
        lua_pop(state, 1);
        return luaL_error(state,
                          "Native callback owner for '%s' has been disposed",
                          methodName);
    }
    sol::state_view lua(state);
    const sol::object instance = sol::stack::get<sol::object>(state, -1);
    const sol::object rawClass = actualClassOf(lua, instance);
    const sol::object callback =
        rawClass.is<sol::table>()
            ? findScriptMember(lua, rawClass.as<sol::table>(),
                               sol::make_object(lua, methodName))
            : nilObject(lua);
    if (!callback.is<sol::function>()) {
        return luaL_error(state, "Native callback method '%s' is not defined",
                          methodName);
    }
    callback.push();
    lua_insert(state, 1);
    lua_insert(state, 2);
    if (ludork::standard::protectedLuaCall(state, argumentCount + 1,
                                           LUA_MULTRET) != LUA_OK) {
        const std::string message =
            ludork::standard::luaErrorMessage(state, -1);
        return luaL_error(state, "Native callback '%s' failed: %s", methodName,
                          message.c_str());
    }
    return lua_gettop(state);
}

int nativeSelf(lua_State* state) {
    pushNativeCallbackInstance(state);
    return 1;
}

sol::table nativeCallbacks(sol::state_view lua, const sol::table& nativeType,
                           const sol::object& instance) {
    sol::table result = lua.create_table();
    sol::table holder = createWeakTable(lua, "k");
    holder.raw_set(instance, true);
    holder.push();
    lua_pushcclosure(lua.lua_state(), nativeSelf, 1);
    sol::object selfResolver =
        sol::stack::get<sol::object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    result.raw_set("__self", selfResolver);
    const sol::object rawNames =
        rawMember(lua, nativeType, sol::make_object(lua, "__classCallbacks"));
    if (!rawNames.is<sol::table>()) {
        return result;
    }
    const sol::table names = rawNames.as<sol::table>();
    const sol::object rawClass = actualClassOf(lua, instance);
    for (std::size_t index = 1; index <= names.size(); ++index) {
        const sol::object rawName = names[index];
        if (!rawName.is<std::string>()) {
            continue;
        }
        const std::string name = rawName.as<std::string>();
        if (!rawClass.is<sol::table>() ||
            !findScriptMember(lua, rawClass.as<sol::table>(), rawName)
                 .is<sol::function>()) {
            continue;
        }
        holder.push();
        lua_pushlstring(lua.lua_state(), name.c_str(), name.size());
        lua_pushcclosure(lua.lua_state(), nativeCallback, 2);
        sol::object callback =
            sol::stack::get<sol::object>(lua.lua_state(), -1);
        lua_pop(lua.lua_state(), 1);
        result.raw_set(name, callback);
    }
    return result;
}

// ── Native object management ──────────────────────────────────────────────────

sol::object invokeNativeFactory(sol::state_view lua,
                                const sol::table& nativeType,
                                const sol::object& instance,
                                const sol::object& rawArguments) {
    sol::object rawFactory =
        rawMember(lua, nativeType, sol::make_object(lua, "__classFactory"));
    const bool isClassFactory = rawFactory.is<sol::protected_function>();
    if (!isClassFactory) {
        rawFactory = rawMember(lua, nativeType, sol::make_object(lua, "new"));
    }
    if (!rawFactory.is<sol::protected_function>()) {
        throw std::runtime_error("Native base " +
                                 nativeTypeName(lua, nativeType) +
                                 " has no class factory");
    }
    lua_State* state = lua.lua_state();
    std::vector<sol::object> arguments;
    if (isClassFactory) {
        arguments.push_back(nativeCallbacks(lua, nativeType, instance));
    }
    if (rawArguments.valid() && rawArguments.get_type() != sol::type::lua_nil) {
        if (!rawArguments.is<sol::table>()) {
            throw std::invalid_argument("Native constructor arguments for " +
                                        nativeTypeName(lua, nativeType) +
                                        " must be a packed table");
        }
        const sol::table packed = rawArguments.as<sol::table>();
        const sol::object rawCount = packed.raw_get<sol::object>("n");
        const lua_Integer count =
            rawCount.is<lua_Integer>()
                ? rawCount.as<lua_Integer>()
                : static_cast<lua_Integer>(packed.size());
        if (count < 0) {
            throw std::invalid_argument(
                "Native constructor argument count cannot be negative");
        }
        if (static_cast<std::uint64_t>(count) >
            static_cast<std::uint64_t>(INT_MAX) - arguments.size()) {
            throw std::length_error(
                "Native constructor argument count overflow");
        }
        arguments.reserve(arguments.size() +
                          static_cast<std::size_t>(count));
        for (lua_Integer index = 1; index <= count; ++index) {
            arguments.push_back(packed.raw_get<sol::object>(index));
        }
    }
    const int stackBase = lua_gettop(state);
    sol::object nativeObject;
    try {
        const int resultCount = invokeRuntimeFunction(
            lua, rawFactory, arguments, "native class factory arguments");
        nativeObject =
            resultCount == 0
                ? nilObject(lua)
                : sol::stack::get<sol::object>(state, stackBase + 1);
        lua_settop(state, stackBase);
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
    if (nativeObject.get_type() != sol::type::userdata) {
        throw std::runtime_error("Native class factory for " +
                                 nativeTypeName(lua, nativeType) +
                                 " did not return userdata");
    }
    return nativeObject;
}

void validateNativeObject(sol::state_view lua, const sol::table& root,
                          const sol::object& nativeObject) {
    const sol::table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const sol::object rawType = nativeMro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!nativeTypeAccepts(lua, nativeType, nativeObject)) {
            throw std::runtime_error(
                "Native factory for " + nativeTypeName(lua, root) +
                " does not implement " + nativeTypeName(lua, nativeType));
        }
    }
}

void addNativeObject(sol::state_view lua, sol::table nativeObjects,
                     const sol::table& root, const sol::object& nativeObject) {
    validateNativeObject(lua, root, nativeObject);
    const sol::table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const sol::object rawType = nativeMro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        const std::string name = nativeTypeName(lua, nativeType);
        const sol::object current = nativeObjects.raw_get<sol::object>(name);
        if (!current.valid() || current.get_type() == sol::type::lua_nil) {
            nativeObjects.raw_set(name, nativeObject);
        }
    }
}

void replaceNativeObject(sol::state_view lua, sol::table nativeObjects,
                         const sol::table& root, const sol::object& previous,
                         const sol::object& nativeObject) {
    validateNativeObject(lua, root, nativeObject);
    const sol::table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const sol::object rawType = nativeMro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const std::string name =
            nativeTypeName(lua, rawType.as<sol::table>());
        const sol::object current = nativeObjects.raw_get<sol::object>(name);
        if (!current.valid() || current.get_type() == sol::type::lua_nil ||
            objectsRawEqual(current, previous)) {
            nativeObjects.raw_set(name, nativeObject);
        }
    }
}

void removeNativeObject(sol::state_view lua, sol::table nativeObjects,
                        const sol::table& root,
                        const sol::object& nativeObject) {
    const sol::table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const sol::object rawType = nativeMro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const std::string name =
            nativeTypeName(lua, rawType.as<sol::table>());
        const sol::object current = nativeObjects.raw_get<sol::object>(name);
        if (objectsRawEqual(current, nativeObject)) {
            nativeObjects.raw_set(name, sol::lua_nil);
        }
    }
}

void registerNativeOwner(sol::state_view lua, const sol::object& nativeObject,
                         const sol::object& instance) {
    registryTable(lua, NATIVE_OWNERS_KEY, "kv").raw_set(nativeObject,
                                                         instance);
    registerNativePointerOwner(lua, nativeObject, instance);
}

void unregisterNativeOwner(sol::state_view lua, const sol::object& nativeObject,
                           const sol::object& owner) {
    sol::table owners = registryTable(lua, NATIVE_OWNERS_KEY, "kv");
    const sol::object current = owners.raw_get<sol::object>(nativeObject);
    if (!objectsRawEqual(current, owner)) {
        return;
    }
    owners.raw_set(nativeObject, sol::lua_nil);
    unregisterNativePointerOwner(lua, nativeObject, owner);
}

void clearNativeMethodCaches(sol::state_view lua, const sol::object& instance,
                             sol::table fields) {
    fields.raw_set(NATIVE_METHOD_CACHE_FIELD, sol::lua_nil);
    registryTable(lua, SUPER_PROXY_CACHE_KEY, "k")
        .raw_set(instance, sol::lua_nil);
}

sol::table nativeRootsUnderConstruction(sol::state_view lua,
                                        sol::table fields) {
    const sol::object rawRoots =
        fields.raw_get<sol::object>(NATIVE_CONSTRUCTING_ROOTS_FIELD);
    if (rawRoots.is<sol::table>()) {
        return rawRoots.as<sol::table>();
    }
    sol::table roots = lua.create_table();
    fields.raw_set(NATIVE_CONSTRUCTING_ROOTS_FIELD, roots);
    return roots;
}

void beginNativeRootConstruction(sol::state_view lua, sol::table fields,
                                 const sol::table& root) {
    sol::table roots = nativeRootsUnderConstruction(lua, fields);
    const sol::object active = roots.raw_get<sol::object>(root);
    if (active.is<bool>() && active.as<bool>()) {
        throw std::runtime_error("Recursive native root construction for " +
                                 nativeTypeName(lua, root));
    }
    roots.raw_set(root, true);
}

void endNativeRootConstruction(sol::table fields, const sol::table& root) {
    const sol::object rawRoots =
        fields.raw_get<sol::object>(NATIVE_CONSTRUCTING_ROOTS_FIELD);
    if (!rawRoots.is<sol::table>()) {
        return;
    }
    sol::table roots = rawRoots.as<sol::table>();
    roots.raw_set(root, sol::lua_nil);
    if (tableIsEmpty(roots)) {
        fields.raw_set(NATIVE_CONSTRUCTING_ROOTS_FIELD, sol::lua_nil);
    }
}

}  // namespace

sol::object constructNativeRoot(sol::state_view lua,
                                const sol::table& classTable,
                                const sol::object& instance,
                                const sol::table& root,
                                const sol::object& arguments) {
    sol::table fields = class_native::getUserFields(lua, instance, false);
    const sol::object rawObjects =
        fields.raw_get<sol::object>("__nativeObjects");
    const sol::object rawInstanceId =
        fields.raw_get<sol::object>("__instanceId");
    if (!rawObjects.is<sol::table>() || !rawInstanceId.is<std::size_t>()) {
        throw std::runtime_error(
            "Composite instance has incomplete native state");
    }
    sol::table nativeObjects = rawObjects.as<sol::table>();
    beginNativeRootConstruction(lua, fields, root);
    sol::object nativeObject = nilObject(lua);
    NativeShadowSnapshot shadowSnapshot;
    try {
        nativeObject = invokeNativeFactory(lua, root, instance, arguments);
        addNativeObject(lua, nativeObjects, root, nativeObject);
        registerNativeOwner(lua, nativeObject, instance);
        syncNativeRootDefaults(lua, classTable, instance, root, nativeObject,
                               shadowSnapshot);
    } catch (...) {
        if (nativeObject.is<sol::userdata>()) {
            removeNativeObject(lua, nativeObjects, root, nativeObject);
            unregisterNativeOwner(lua, nativeObject, instance);
        }
        restoreNativeShadows(fields, shadowSnapshot);
        endNativeRootConstruction(fields, root);
        clearNativeMethodCaches(lua, instance, fields);
        throw;
    }
    endNativeRootConstruction(fields, root);
    clearNativeMethodCaches(lua, instance, fields);
    return nativeObject;
}

namespace {

int nativeBaseInitializer(lua_State* state) {
    try {
        sol::state_view lua(state);
        if (lua_gettop(state) < 1) {
            throw std::invalid_argument(
                "Native base initializer requires a class instance");
        }
        const sol::table nativeType =
            sol::stack::get<sol::table>(state, lua_upvalueindex(1));
        const sol::object instance = sol::stack::get<sol::object>(state, 1);
        if (!isCompositeInstance(lua, instance)) {
            throw std::invalid_argument(
                "Native base initializer requires a composite class instance");
        }
        sol::table fields = class_native::getUserFields(lua, instance, false);
        const sol::object rawInitializing =
            fields.raw_get<sol::object>(NATIVE_INITIALIZING_FIELD);
        if (!rawInitializing.is<bool>() || !rawInitializing.as<bool>()) {
            throw std::runtime_error(
                "Native base initializers may only run during class "
                "construction");
        }
        const sol::object rawClass = fields.raw_get<sol::object>("__class");
        if (!rawClass.is<sol::table>()) {
            throw std::runtime_error("Composite instance has no class");
        }
        const sol::table classTable = rawClass.as<sol::table>();
        bool knownRoot = false;
        for (const sol::table& root : nativeRoots(lua, classTable)) {
            if (objectsRawEqual(root, nativeType)) {
                knownRoot = true;
                break;
            }
        }
        if (!knownRoot) {
            throw std::invalid_argument(
                "Native initializer target is not an exact class root");
        }
        const sol::object rawInitialized =
            fields.raw_get<sol::object>("__classInitializedRoots");
        sol::table initialized = rawInitialized.is<sol::table>()
                                     ? rawInitialized.as<sol::table>()
                                     : lua.create_table();
        if (!rawInitialized.is<sol::table>()) {
            fields.raw_set("__classInitializedRoots", initialized);
        }
        const sol::object alreadyInitialized =
            initialized.raw_get<sol::object>(nativeType);
        if (alreadyInitialized.is<bool>() && alreadyInitialized.as<bool>()) {
            throw std::runtime_error(
                "Native base initializer was called twice for " +
                nativeTypeName(lua, nativeType));
        }
        const sol::object rawObjects =
            fields.raw_get<sol::object>("__nativeObjects");
        if (!rawObjects.is<sol::table>()) {
            throw std::runtime_error(
                "Composite instance has no native object map");
        }
        sol::table nativeObjects = rawObjects.as<sol::table>();
        const sol::object previous =
            nativeObjects.raw_get<sol::object>(nativeTypeName(lua, nativeType));
        const int argumentCount = lua_gettop(state) - 1;
        if (previous.is<sol::userdata>() && argumentCount == 0) {
            initialized.raw_set(nativeType, true);
            return 0;
        }
        sol::table arguments = lua.create_table();
        arguments.raw_set("n", argumentCount);
        for (int index = 0; index < argumentCount; ++index) {
            arguments.raw_set(index + 1,
                              sol::stack::get<sol::object>(state, index + 2));
        }
        if (!previous.is<sol::userdata>()) {
            constructNativeRoot(lua, classTable, instance, nativeType,
                                arguments);
            initialized.raw_set(nativeType, true);
            return 0;
        }
        const sol::object rawInstanceId =
            fields.raw_get<sol::object>("__instanceId");
        if (!rawInstanceId.is<std::size_t>()) {
            throw std::runtime_error(
                "Composite instance has no native instance id");
        }
        beginNativeRootConstruction(lua, fields, nativeType);
        sol::object nativeObject = nilObject(lua);
        NativeShadowSnapshot shadowSnapshot;
        bool replaced = false;
        try {
            nativeObject =
                invokeNativeFactory(lua, nativeType, instance, arguments);
            validateNativeObject(lua, nativeType, nativeObject);
            replaceNativeObject(lua, nativeObjects, nativeType, previous,
                                nativeObject);
            replaced = true;
            registerNativeOwner(lua, nativeObject, instance);
            syncNativeRootDefaults(lua, classTable, instance, nativeType,
                                   nativeObject, shadowSnapshot);
            replayNativeDirtyProperties(lua, fields, nativeType, previous,
                                        nativeObject);
        } catch (...) {
            if (replaced) {
                replaceNativeObject(lua, nativeObjects, nativeType,
                                    nativeObject, previous);
                unregisterNativeOwner(lua, nativeObject, instance);
                registerNativeOwner(lua, previous, instance);
            }
            restoreNativeShadows(fields, shadowSnapshot);
            endNativeRootConstruction(fields, nativeType);
            clearNativeMethodCaches(lua, instance, fields);
            throw;
        }
        unregisterNativeOwner(lua, previous, instance);
        registerNativeOwner(lua, nativeObject, instance);
        endNativeRootConstruction(fields, nativeType);
        clearNativeMethodCaches(lua, instance, fields);
        initialized.raw_set(nativeType, true);
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

}  // namespace

void ensureNativeInitializer(sol::state_view lua, sol::table nativeType) {
    const sol::object rawFactory =
        rawMember(lua, nativeType, sol::make_object(lua, "__classFactory"));
    const sol::object rawNew =
        rawMember(lua, nativeType, sol::make_object(lua, "new"));
    if (!rawFactory.is<sol::protected_function>() &&
        !rawNew.is<sol::protected_function>()) {
        return;
    }
    sol::object initializer =
        nativeType.raw_get<sol::object>(NATIVE_INITIALIZER_FIELD);
    if (!initializer.is<sol::function>()) {
        nativeType.push();
        lua_pushcclosure(lua.lua_state(), nativeBaseInitializer, 1);
        initializer = sol::stack::get<sol::object>(lua.lua_state(), -1);
        lua_pop(lua.lua_state(), 1);
        nativeType.raw_set(NATIVE_INITIALIZER_FIELD, initializer);
    }
    const sol::object existing =
        rawMember(lua, nativeType, sol::make_object(lua, "init"));
    if (!existing.valid() || existing.get_type() == sol::type::lua_nil) {
        nativeType.raw_set("init", initializer);
    }
}

// ── Instance validation ───────────────────────────────────────────────────────

void validateNativeInstanceShape(sol::state_view lua,
                                 const sol::table& classTable,
                                 const sol::object& instance) {
    const std::vector<sol::table> roots = nativeRoots(lua, classTable);
    if (roots.empty()) {
        return;
    }
    if (!isCompositeInstance(lua, instance)) {
        throw std::runtime_error(
            "Class with native bases must return its composite instance");
    }
    const sol::table fields = class_native::getUserFields(lua, instance, false);
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    if (!rawClass.is<sol::table>() ||
        !objectsRawEqual(rawClass.as<sol::table>(), classTable)) {
        throw std::runtime_error("Composite instance belongs to another class");
    }
}

void validateNativeRoots(sol::state_view lua, const sol::table& classTable,
                         const sol::object& instance) {
    validateNativeInstanceShape(lua, classTable, instance);
    const std::vector<sol::table> roots = nativeRoots(lua, classTable);
    if (roots.empty()) {
        return;
    }
    const sol::table fields = class_native::getUserFields(lua, instance, false);
    for (const sol::table& root : roots) {
        if (!nativeObjectForType(lua, fields, root).is<sol::userdata>()) {
            throw std::runtime_error("Lua class initializer must call " +
                                     nativeTypeName(lua, root) +
                                     ".init(self, ...)");
        }
    }
}

bool compositeBelongsToClass(sol::state_view lua, const sol::object& instance,
                             const sol::table& classTable) {
    if (!isCompositeInstance(lua, instance)) {
        return false;
    }
    const sol::object rawClass =
        class_native::getUserFields(lua, instance, false)
            .raw_get<sol::object>("__class");
    return rawClass.is<sol::table>() &&
           objectsRawEqual(rawClass.as<sol::table>(), classTable);
}

// ── Instance lifecycle ────────────────────────────────────────────────────────

namespace {

bool tableMatchesInstanceClass(sol::state_view lua, const sol::table& instance,
                               const sol::table& classTable) {
    if (objectsRawEqual(instance, classTable)) {
        return false;
    }
    const sol::table metatable =
        class_native::getObjectMetatable(lua, sol::make_object(lua, instance));
    if (objectsRawEqual(metatable, classTable)) {
        return true;
    }
    const sol::object rawMonitor = registryTable(lua, MONITOR_STATES_KEY, "k")
                                       .raw_get<sol::object>(instance);
    if (!rawMonitor.is<sol::table>()) {
        return false;
    }
    const sol::object originalMetatable =
        rawMonitor.as<sol::table>().raw_get<sol::object>("meta");
    return originalMetatable.is<sol::table>() &&
           objectsRawEqual(originalMetatable.as<sol::table>(), classTable);
}

}  // namespace

std::optional<sol::table> tryManagedInstanceFields(sol::state_view lua,
                                                    const sol::object& instance) {
    if (isCompositeInstance(lua, instance)) {
        const sol::table fields =
            class_native::getUserFields(lua, instance, false);
        const sol::object rawClass = fields.raw_get<sol::object>("__class");
        if (rawClass.is<sol::table>() && isClass(rawClass.as<sol::table>())) {
            return fields;
        }
    } else if (instance.is<sol::table>()) {
        const sol::table fields = instance.as<sol::table>();
        const sol::object rawClass = fields.raw_get<sol::object>("__class");
        if (rawClass.is<sol::table>() && isClass(rawClass.as<sol::table>()) &&
            tableMatchesInstanceClass(lua, fields,
                                      rawClass.as<sol::table>())) {
            return fields;
        }
    }
    return std::nullopt;
}

sol::table managedInstanceFields(sol::state_view lua,
                                 const sol::object& instance) {
    const std::optional<sol::table> fields =
        tryManagedInstanceFields(lua, instance);
    if (fields.has_value()) {
        return *fields;
    }
    throw std::invalid_argument(
        "Class lifecycle requires a Ludork class instance");
}

namespace {

LifecycleState lifecycleState(sol::state_view lua,
                              const sol::object& instance) {
    const sol::object rawState = registryTable(lua, LIFECYCLE_STATES_KEY, "k")
                                     .raw_get<sol::object>(instance);
    if (rawState.is<lua_Integer>()) {
        const lua_Integer value = rawState.as<lua_Integer>();
        if (value == static_cast<lua_Integer>(LifecycleState::Disposing)) {
            return LifecycleState::Disposing;
        }
        if (value == static_cast<lua_Integer>(LifecycleState::Disposed)) {
            return LifecycleState::Disposed;
        }
    }
    return LifecycleState::Active;
}

void setLifecycleState(sol::state_view lua, const sol::object& instance,
                       LifecycleState state) {
    registryTable(lua, LIFECYCLE_STATES_KEY, "k")
        .raw_set(instance, static_cast<lua_Integer>(state));
}

int disposedInstanceAccess(lua_State* state) {
    return luaL_error(state, "Class instance has been disposed");
}

sol::table disposedInstanceMetatable(sol::state_view lua) {
    sol::table registry = lua.registry();
    const sol::object rawMetatable =
        registry.raw_get<sol::object>(DISPOSED_METATABLE_KEY);
    if (rawMetatable.is<sol::table>()) {
        return rawMetatable.as<sol::table>();
    }
    sol::table metatable = lua.create_table();
    metatable.push();
    lua_pushcfunction(lua.lua_state(), disposedInstanceAccess);
    lua_setfield(lua.lua_state(), -2, "__index");
    lua_pushcfunction(lua.lua_state(), disposedInstanceAccess);
    lua_setfield(lua.lua_state(), -2, "__newindex");
    lua_pop(lua.lua_state(), 1);
    metatable.raw_set("__metatable", "disposed");
    registry.raw_set(DISPOSED_METATABLE_KEY, metatable);
    return metatable;
}

void protectDisposedInstance(sol::state_view lua, const sol::object& instance) {
    instance.push();
    disposedInstanceMetatable(lua).push();
    lua_setmetatable(lua.lua_state(), -2);
    lua_pop(lua.lua_state(), 1);
}

}  // namespace

void reportDisposeError(const char* phase, const std::string& message) {
    std::fprintf(stderr, "Class dispose %s failed: %s\n", phase,
                 message.c_str());
}

namespace {

template <typename Callback>
void runDisposePhase(const char* phase, Callback&& callback) noexcept {
    try {
        callback();
    } catch (const std::exception& error) {
        reportDisposeError(phase, error.what());
    } catch (...) {
        reportDisposeError(phase, "unknown error");
    }
}

void invokeClassDispose(sol::state_view lua, const sol::object& instance,
                        const sol::table& classTable) {
    const sol::object dispose =
        findScriptMember(lua, classTable, sol::make_object(lua, "dispose"));
    if (!dispose.is<sol::function>()) {
        return;
    }
    dispose.push();
    instance.push();
    if (ludork::standard::protectedLuaCall(lua.lua_state(), 1, 0) != LUA_OK) {
        reportDisposeError("dispose", popLuaError(lua.lua_state(),
                                                  "Lua class dispose failed"));
    }
}

void clearInstanceMonitor(sol::state_view lua, const sol::object& instance) {
    sol::table states = registryTable(lua, MONITOR_STATES_KEY, "k");
    const sol::object rawMonitor = states.raw_get<sol::object>(instance);
    if (!instance.is<sol::table>() || !rawMonitor.is<sol::table>()) {
        states.raw_set(instance, sol::lua_nil);
        return;
    }
    const sol::object originalMetatable =
        rawMonitor.as<sol::table>().raw_get<sol::object>("meta");
    instance.push();
    if (originalMetatable.valid() &&
        originalMetatable.get_type() != sol::type::lua_nil) {
        originalMetatable.push();
    } else {
        lua_pushnil(lua.lua_state());
    }
    lua_setmetatable(lua.lua_state(), -2);
    lua_pop(lua.lua_state(), 1);
    states.raw_set(instance, sol::lua_nil);
}

struct NativeDisposeTarget {
    sol::table root;
    sol::object nativeObject;
    bool requiresHook{};
};

struct DisposeSnapshot {
    sol::table fields;
    sol::table classTable;
    std::optional<std::size_t> instanceId;
    std::vector<NativeDisposeTarget> nativeTargets;
};

DisposeSnapshot createDisposeSnapshot(sol::state_view lua,
                                      const sol::object& instance) {
    const sol::table fields = managedInstanceFields(lua, instance);
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    if (!rawClass.is<sol::table>()) {
        throw std::runtime_error("Class instance has no runtime class");
    }
    DisposeSnapshot snapshot{
        fields,
        rawClass.as<sol::table>(),
        std::nullopt,
        {},
    };
    const sol::object rawInstanceId =
        fields.raw_get<sol::object>("__instanceId");
    if (rawInstanceId.is<std::size_t>()) {
        snapshot.instanceId = rawInstanceId.as<std::size_t>();
    }
    for (const sol::table& root : nativeRoots(lua, snapshot.classTable)) {
        const sol::object nativeObject =
            nativeObjectForType(lua, fields, root);
        if (!nativeObject.is<sol::userdata>()) {
            continue;
        }
        const sol::object rawCallbacks =
            root.raw_get<sol::object>("__classCallbacks");
        const sol::object rawMetadataModule =
            root.raw_get<sol::object>("__metadataModule");
        snapshot.nativeTargets.push_back({
            root,
            nativeObject,
            rawCallbacks.is<sol::table>() &&
                !tableIsEmpty(rawCallbacks.as<sol::table>()) &&
                rawMetadataModule.is<std::string>(),
        });
    }
    return snapshot;
}

void invokeNativeDisposeHooks(sol::state_view lua, const sol::object& instance,
                              const std::vector<NativeDisposeTarget>& targets) {
    for (const NativeDisposeTarget& target : targets) {
        runDisposePhase("native hook", [&]() {
            const sol::object rawDispose = rawMember(
                lua, target.root, sol::make_object(lua, "__classRelease"));
            if (rawDispose.is<sol::protected_function>()) {
                sol::protected_function_result disposed =
                    rawDispose.as<sol::protected_function>()(
                        target.nativeObject);
                if (!disposed.valid()) {
                    const sol::error error = disposed;
                    reportDisposeError("native hook", error.what());
                }
            } else if (target.requiresHook) {
                reportDisposeError("native hook",
                                   "Missing __classRelease for " +
                                       nativeTypeName(lua, target.root));
            }
        });
        runDisposePhase("native owner", [&]() {
            unregisterNativeOwner(lua, target.nativeObject, instance);
        });
    }
}

void clearInstanceFields(sol::table fields) {
    std::vector<sol::object> keys;
    for (const auto& entry : fields) {
        keys.push_back(entry.first);
    }
    for (const sol::object& key : keys) {
        fields.raw_set(key, sol::lua_nil);
    }
}

}  // namespace

bool disposeInstanceCore(sol::state_view lua, const sol::object& instance,
                         bool invokeDispose) {
    if (lifecycleState(lua, instance) != LifecycleState::Active) {
        return false;
    }
    DisposeSnapshot snapshot = createDisposeSnapshot(lua, instance);
    setLifecycleState(lua, instance, LifecycleState::Disposing);
    if (invokeDispose) {
        runDisposePhase("dispose", [&]() {
            invokeClassDispose(lua, instance, snapshot.classTable);
        });
    }
    runDisposePhase("monitor", [&]() {
        clearInstanceMonitor(lua, instance);
    });
    runDisposePhase("native roots", [&]() {
        invokeNativeDisposeHooks(lua, instance, snapshot.nativeTargets);
    });
    runDisposePhase("instance registry", [&]() {
        if (snapshot.instanceId.has_value()) {
            registryTable(lua, INSTANCES_KEY, "v")
                .raw_set(*snapshot.instanceId, sol::lua_nil);
        }
    });
    runDisposePhase("method cache", [&]() {
        clearNativeMethodCaches(lua, instance, snapshot.fields);
    });
    runDisposePhase("instance fields", [&]() {
        clearInstanceFields(snapshot.fields);
    });
    runDisposePhase("disposed metatable", [&]() {
        protectDisposedInstance(lua, instance);
    });
    setLifecycleState(lua, instance, LifecycleState::Disposed);
    return true;
}

int classInstanceGc(lua_State* state) {
    try {
        if (lua_gettop(state) >= 1) {
            sol::state_view lua(state);
            const sol::object shuttingDown =
                lua.registry().raw_get<sol::object>(SHUTTING_DOWN_KEY);
            if (!(shuttingDown.is<bool>() && shuttingDown.as<bool>())) {
                disposeInstanceCore(lua, sol::stack::get<sol::object>(state, 1),
                                    true);
            }
        }
    } catch (const std::exception& error) {
        reportDisposeError("__gc", error.what());
    } catch (...) {
        reportDisposeError("__gc", "unknown error");
    }
    return 0;
}

void failNativeConstruction(sol::state_view lua, const sol::table& classTable,
                            const sol::object& instance) {
    const sol::object rawClass = scriptClassOf(lua, instance);
    if (!rawClass.is<sol::table>() ||
        !objectsRawEqual(rawClass.as<sol::table>(), classTable)) {
        return;
    }
    runDisposePhase("failed construction", [&]() {
        disposeInstanceCore(lua, instance, false);
    });
}

void finishNativeConstruction(sol::state_view lua, const sol::table& classTable,
                              const sol::object& instance) {
    if (!compositeBelongsToClass(lua, instance, classTable)) {
        return;
    }
    sol::table fields = class_native::getUserFields(lua, instance, false);
    fields.raw_set(NATIVE_INITIALIZING_FIELD, false);
    fields.raw_set(NATIVE_CONSTRUCTION_FAILED_FIELD, sol::lua_nil);
    fields.raw_set("__classInitializedRoots", sol::lua_nil);
    fields.raw_set(NATIVE_CONSTRUCTING_ROOTS_FIELD, sol::lua_nil);
    fields.raw_set(NATIVE_DIRTY_PROPERTIES_FIELD, sol::lua_nil);
    instance.push();
    compositeMetatable(lua).push();
    lua_setmetatable(lua.lua_state(), -2);
    lua_pop(lua.lua_state(), 1);
}

void completeDefaultNativeRoots(sol::state_view lua,
                                const sol::table& classTable,
                                const sol::object& instance) {
    if (!isCompositeInstance(lua, instance)) {
        return;
    }
    sol::table fields = class_native::getUserFields(lua, instance, false);
    for (const sol::table& root : nativeRoots(lua, classTable)) {
        if (nativeObjectForType(lua, fields, root).is<sol::userdata>()) {
            continue;
        }
        const sol::object rawMinimum =
            root.raw_get<sol::object>("__classFactoryMinArgs");
        if (!rawMinimum.is<lua_Integer>() ||
            rawMinimum.as<lua_Integer>() != 0) {
            continue;
        }
        constructNativeRoot(lua, classTable, instance, root, nilObject(lua));
    }
}

sol::object ensureDefaultNativeObject(sol::state_view lua,
                                      const sol::object& instance,
                                      const sol::table& nativeType) {
    if (!isCompositeInstance(lua, instance)) {
        return nilObject(lua);
    }
    sol::table fields = class_native::getUserFields(lua, instance, false);
    sol::object nativeObject = nativeObjectForType(lua, fields, nativeType);
    if (nativeObject.is<sol::userdata>()) {
        return nativeObject;
    }
    if (!rawBool(fields, NATIVE_INITIALIZING_FIELD)) {
        return nilObject(lua);
    }
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    const sol::object rawObjects =
        fields.raw_get<sol::object>("__nativeObjects");
    const sol::object rawInstanceId =
        fields.raw_get<sol::object>("__instanceId");
    if (!rawClass.is<sol::table>() || !rawObjects.is<sol::table>() ||
        !rawInstanceId.is<std::size_t>()) {
        return nilObject(lua);
    }
    const sol::table classTable = rawClass.as<sol::table>();
    sol::table root = lua.create_table();
    bool foundRoot = false;
    for (const sol::table& candidate : nativeRoots(lua, classTable)) {
        if (objectsRawEqual(candidate, nativeType) ||
            derivesFrom(lua, candidate, nativeType)) {
            root = candidate;
            foundRoot = true;
            break;
        }
    }
    if (!foundRoot) {
        return nilObject(lua);
    }
    const sol::object rawMinimum =
        root.raw_get<sol::object>("__classFactoryMinArgs");
    if (!rawMinimum.is<lua_Integer>() || rawMinimum.as<lua_Integer>() != 0) {
        return nilObject(lua);
    }
    constructNativeRoot(lua, classTable, instance, root, nilObject(lua));
    return nativeObjectForType(lua, fields, nativeType);
}

// ── Instance allocation ───────────────────────────────────────────────────────

bool nativeRootIsDeferred(const sol::table& root) {
    const sol::object rawMinimum =
        root.raw_get<sol::object>("__classFactoryMinArgs");
    return rawMinimum.is<lua_Integer>() && rawMinimum.as<lua_Integer>() >= 0;
}

namespace {

sol::object createNativeInstance(sol::state_view lua,
                                 const sol::table& classTable,
                                 const sol::object& rawConstructorArguments,
                                 bool allowDeferredRoots) {
    const std::vector<sol::table> roots = nativeRoots(lua, classTable);
    if (roots.empty()) {
        return nilObject(lua);
    }
    sol::table constructorArguments = lua.create_table();
    if (rawConstructorArguments.valid() &&
        rawConstructorArguments.get_type() != sol::type::lua_nil) {
        if (!rawConstructorArguments.is<sol::table>()) {
            throw std::invalid_argument(
                "Class allocator expects a native constructor argument map");
        }
        constructorArguments = rawConstructorArguments.as<sol::table>();
        for (const auto& entry : constructorArguments) {
            if (!entry.first.is<sol::table>()) {
                throw std::invalid_argument(
                    "Native constructor map keys must be native root types");
            }
            bool knownRoot = false;
            for (const sol::table& root : roots) {
                if (objectsRawEqual(entry.first.as<sol::table>(), root)) {
                    knownRoot = true;
                    break;
                }
            }
            if (!knownRoot) {
                throw std::invalid_argument(
                    "Native constructor map contains a type that is not a "
                    "native root");
            }
            if (!entry.second.is<sol::table>()) {
                throw std::invalid_argument(
                    "Native constructor map values must be packed argument "
                    "tables");
            }
        }
    }
    sol::table fields = lua.create_table();
    sol::table nativeObjects = lua.create_table();
    fields.raw_set("__class", classTable);
    fields.raw_set("__nativeObjects", nativeObjects);
    const std::size_t instanceId = class_native::nextInstanceId(lua);
    fields.raw_set("__instanceId", instanceId);
    fields.raw_set(NATIVE_INITIALIZING_FIELD, true);
    lua_newuserdatauv(lua.lua_state(), 1, 1);
    constructingCompositeMetatable(lua).push();
    lua_setmetatable(lua.lua_state(), -2);
    fields.push();
    lua_setiuservalue(lua.lua_state(), -2, 1);
    sol::object instance = sol::stack::get<sol::object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    registryTable(lua, INSTANCES_KEY, "v").raw_set(instanceId, instance);
    try {
        for (const sol::table& root : roots) {
            const sol::object arguments =
                constructorArguments.raw_get<sol::object>(root);
            if (allowDeferredRoots && !arguments.is<sol::table>() &&
                nativeRootIsDeferred(root)) {
                continue;
            }
            constructNativeRoot(lua, classTable, instance, root, arguments);
        }
    } catch (...) {
        failNativeConstruction(lua, classTable, instance);
        throw;
    }
    return instance;
}

}  // namespace

sol::object allocateInstance(sol::state_view lua, const sol::table& classTable,
                             const sol::object& constructorArguments,
                             bool allowDeferredRoots) {
    sol::object instance = createNativeInstance(
        lua, classTable, constructorArguments, allowDeferredRoots);
    if (!instance.valid() || instance.get_type() == sol::type::lua_nil) {
        sol::table tableInstance = lua.create_table();
        tableInstance.raw_set("__class", classTable);
        tableInstance[sol::metatable_key] = classTable;
        instance = tableInstance;
    }
    return instance;
}

}  // namespace ludork::standard::class_runtime::detail
