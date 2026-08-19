#include "Instance/InstanceRuntime.hpp"

#include "Composite/CompositeRuntime.hpp"
#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeBridge.hpp"
#include "Detail/RuntimeState.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <LuaError.hpp>
#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <climits>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ludork::standard::class_runtime::detail {

// ── Native callback support
// ───────────────────────────────────────────────────

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

// ── Native object management
// ──────────────────────────────────────────────────

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
        const lua_Integer count = rawCount.is<lua_Integer>()
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
        arguments.reserve(arguments.size() + static_cast<std::size_t>(count));
        for (lua_Integer index = 1; index <= count; ++index) {
            arguments.push_back(packed.raw_get<sol::object>(index));
        }
    }
    const int stackBase = lua_gettop(state);
    sol::object nativeObject;
    try {
        const int resultCount = invokeRuntimeFunction(
            lua, rawFactory, arguments, "native class factory arguments");
        nativeObject = resultCount == 0
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
        const std::string name = nativeTypeName(lua, rawType.as<sol::table>());
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
        const std::string name = nativeTypeName(lua, rawType.as<sol::table>());
        const sol::object current = nativeObjects.raw_get<sol::object>(name);
        if (objectsRawEqual(current, nativeObject)) {
            nativeObjects.raw_set(name, sol::lua_nil);
        }
    }
}

void registerNativeOwner(sol::state_view lua, const sol::object& nativeObject,
                         const sol::object& instance) {
    registryTable(lua, NATIVE_OWNERS_KEY, "kv").raw_set(nativeObject, instance);
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

sol::object constructNativeRoot(sol::state_view lua,
                                const sol::table& classTable,
                                const sol::object& instance,
                                const sol::table& root,
                                const sol::object& arguments) {
    sol::table fields = class_native::getUserFields(lua, instance, false);
    const sol::object rawObjects =
        fields.raw_get<sol::object>(protocol::NATIVE_OBJECTS_FIELD);
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
            fields.raw_get<sol::object>(protocol::NATIVE_OBJECTS_FIELD);
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

}  // namespace ludork::standard::class_runtime::detail
