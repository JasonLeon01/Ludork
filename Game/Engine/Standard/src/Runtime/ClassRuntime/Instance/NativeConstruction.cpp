#include "Instance/InstanceRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeBridge.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypeQueries.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <LuaError.hpp>
#include <LuaGlue/LuaGlue.hpp>

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
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        const int argumentCount = lua_gettop(state);
        const char* methodName = lua_tostring(state, lua_upvalueindex(2));
        if (!pushNativeCallbackInstance(state)) {
            lua_pop(state, 1);
            throw std::runtime_error(
                std::string("Native callback owner for '") + methodName +
                "' has been disposed");
        }
        lua_glue::StateView lua(state);
        const lua_glue::Object instance =
            lua_glue::Read<lua_glue::Object>(state, -1);
        const lua_glue::Object rawClass = actualClassOf(lua, instance);
        const lua_glue::Object callback =
            rawClass.is<lua_glue::Table>()
                ? findScriptMember(lua, rawClass.as<lua_glue::Table>(),
                                   lua_glue::MakeObject(lua, methodName))
                : nilObject(lua);
        if (!callback.is<lua_glue::Function>()) {
            throw std::runtime_error(std::string("Native callback method '") +
                                     methodName + "' is not defined");
        }
        callback.push(state);
        lua_insert(state, 1);
        lua_insert(state, 2);
        if (ludork::standard::protectedLuaCall(state, argumentCount + 1,
                                               LUA_MULTRET) != LUA_OK) {
            const std::string message =
                ludork::standard::luaErrorMessage(state, -1);
            throw std::runtime_error(std::string("Native callback '") +
                                     methodName + "' failed: " + message);
        }
        return lua_gettop(state);
    });
}

int nativeSelf(lua_State* state) {
    pushNativeCallbackInstance(state);
    return 1;
}

lua_glue::Table nativeCallbacks(lua_glue::StateView lua,
                                const lua_glue::Table& nativeType,
                                const lua_glue::Object& instance) {
    lua_glue::Table result = lua.create_table();
    lua_glue::Table holder = createWeakTable(lua, "k");
    holder.raw_set(instance, true);
    holder.push(lua.lua_state());
    lua_pushcclosure(lua.lua_state(), nativeSelf, 1);
    lua_glue::Object selfResolver =
        lua_glue::Read<lua_glue::Object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    result.raw_set("__self", selfResolver);
    const lua_glue::Object rawNames = rawMember(
        lua, nativeType, lua_glue::MakeObject(lua, CLASS_CALLBACKS_FIELD));
    if (!rawNames.is<lua_glue::Table>()) {
        return result;
    }
    const lua_glue::Table names = rawNames.as<lua_glue::Table>();
    const lua_glue::Object rawClass = actualClassOf(lua, instance);
    for (std::size_t index = 1; index <= names.size(); ++index) {
        const lua_glue::Object rawName = names[index];
        if (!rawName.is<std::string>()) {
            continue;
        }
        const std::string name = rawName.as<std::string>();
        if (!rawClass.is<lua_glue::Table>() ||
            !findScriptMember(lua, rawClass.as<lua_glue::Table>(), rawName)
                 .is<lua_glue::Function>()) {
            continue;
        }
        holder.push(lua.lua_state());
        lua_pushlstring(lua.lua_state(), name.c_str(), name.size());
        lua_pushcclosure(lua.lua_state(), nativeCallback, 2);
        lua_glue::Object callback =
            lua_glue::Read<lua_glue::Object>(lua.lua_state(), -1);
        lua_pop(lua.lua_state(), 1);
        result.raw_set(name, callback);
    }
    return result;
}

// ── Native object management
// ──────────────────────────────────────────────────

lua_glue::Object invokeNativeFactory(lua_glue::StateView lua,
                                     const lua_glue::Table& nativeType,
                                     const lua_glue::Object& instance,
                                     const lua_glue::Object& rawArguments) {
    lua_glue::Object rawFactory =
        nativeType.raw_get<lua_glue::Object>(CLASS_FACTORY_FIELD);
    const bool isClassFactory = rawFactory.is<lua_glue::Function>();
    if (!isClassFactory) {
        rawFactory = nativeType.raw_get<lua_glue::Object>("new");
    }
    if (!rawFactory.is<lua_glue::Function>()) {
        throw std::runtime_error("Native base " +
                                 nativeTypeName(lua, nativeType) +
                                 " has no class factory");
    }
    lua_State* state = lua.lua_state();
    std::vector<lua_glue::Object> arguments;
    if (isClassFactory) {
        arguments.push_back(nativeCallbacks(lua, nativeType, instance));
    }
    if (rawArguments.valid() &&
        rawArguments.get_type() != lua_glue::Type::Nil) {
        if (!rawArguments.is<lua_glue::Table>()) {
            throw std::invalid_argument("Native constructor arguments for " +
                                        nativeTypeName(lua, nativeType) +
                                        " must be a packed table");
        }
        const lua_glue::Table packed = rawArguments.as<lua_glue::Table>();
        const lua_glue::Object rawCount = packed.raw_get<lua_glue::Object>("n");
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
            arguments.push_back(packed.raw_get<lua_glue::Object>(index));
        }
    }
    const int stackBase = lua_gettop(state);
    lua_glue::Object nativeObject;
    try {
        const int resultCount = invokeRuntimeFunction(
            lua, rawFactory, arguments, "native class factory arguments");
        nativeObject = resultCount == 0 ? nilObject(lua)
                                        : lua_glue::Read<lua_glue::Object>(
                                              state, stackBase + 1);
        lua_settop(state, stackBase);
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
    if (nativeObject.get_type() != lua_glue::Type::Userdata) {
        throw std::runtime_error("Native class factory for " +
                                 nativeTypeName(lua, nativeType) +
                                 " did not return userdata");
    }
    return nativeObject;
}

void validateNativeObject(lua_glue::StateView lua, const lua_glue::Table& root,
                          const lua_glue::Object& nativeObject) {
    const lua_glue::Table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const lua_glue::Object rawType = nativeMro[index];
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table nativeType = rawType.as<lua_glue::Table>();
        if (!nativeTypeAccepts(lua, nativeType, nativeObject)) {
            throw std::runtime_error(
                "Native factory for " + nativeTypeName(lua, root) +
                " does not implement " + nativeTypeName(lua, nativeType));
        }
    }
}

void addNativeObject(lua_glue::StateView lua, lua_glue::Table nativeObjects,
                     const lua_glue::Table& root,
                     const lua_glue::Object& nativeObject) {
    validateNativeObject(lua, root, nativeObject);
    const lua_glue::Table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const lua_glue::Object rawType = nativeMro[index];
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table nativeType = rawType.as<lua_glue::Table>();
        const std::string name = nativeTypeName(lua, nativeType);
        const lua_glue::Object current =
            nativeObjects.raw_get<lua_glue::Object>(name);
        if (!current.valid() || current.get_type() == lua_glue::Type::Nil) {
            nativeObjects.raw_set(name, nativeObject);
        }
    }
}

void replaceNativeObject(lua_glue::StateView lua, lua_glue::Table nativeObjects,
                         const lua_glue::Table& root,
                         const lua_glue::Object& previous,
                         const lua_glue::Object& nativeObject) {
    validateNativeObject(lua, root, nativeObject);
    const lua_glue::Table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const lua_glue::Object rawType = nativeMro[index];
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const std::string name =
            nativeTypeName(lua, rawType.as<lua_glue::Table>());
        const lua_glue::Object current =
            nativeObjects.raw_get<lua_glue::Object>(name);
        if (!current.valid() || current.get_type() == lua_glue::Type::Nil ||
            objectsRawEqual(current, previous)) {
            nativeObjects.raw_set(name, nativeObject);
        }
    }
}

void removeNativeObject(lua_glue::StateView lua, lua_glue::Table nativeObjects,
                        const lua_glue::Table& root,
                        const lua_glue::Object& nativeObject) {
    const lua_glue::Table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const lua_glue::Object rawType = nativeMro[index];
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const std::string name =
            nativeTypeName(lua, rawType.as<lua_glue::Table>());
        const lua_glue::Object current =
            nativeObjects.raw_get<lua_glue::Object>(name);
        if (objectsRawEqual(current, nativeObject)) {
            nativeObjects.raw_set(name, lua_glue::nil);
        }
    }
}

void registerNativeOwner(lua_glue::StateView lua,
                         const lua_glue::Object& nativeObject,
                         const lua_glue::Object& instance) {
    registryTable(lua, NATIVE_OWNERS_KEY, "kv").raw_set(nativeObject, instance);
    registerNativePointerOwner(lua, nativeObject, instance);
}

void unregisterNativeOwner(lua_glue::StateView lua,
                           const lua_glue::Object& nativeObject,
                           const lua_glue::Object& owner) {
    lua_glue::Table owners = registryTable(lua, NATIVE_OWNERS_KEY, "kv");
    const lua_glue::Object current =
        owners.raw_get<lua_glue::Object>(nativeObject);
    if (!objectsRawEqual(current, owner)) {
        return;
    }
    owners.raw_set(nativeObject, lua_glue::nil);
    unregisterNativePointerOwner(lua, nativeObject, owner);
}

void clearNativeMethodCaches(lua_glue::StateView lua,
                             const lua_glue::Object& instance,
                             lua_glue::Table fields) {
    fields.raw_set(NATIVE_METHOD_CACHE_FIELD, lua_glue::nil);
    registryTable(lua, SUPER_PROXY_CACHE_KEY, "k")
        .raw_set(instance, lua_glue::nil);
}

lua_glue::Table nativeRootsUnderConstruction(lua_glue::StateView lua,
                                             lua_glue::Table fields) {
    const lua_glue::Object rawRoots =
        fields.raw_get<lua_glue::Object>(NATIVE_CONSTRUCTING_ROOTS_FIELD);
    if (rawRoots.is<lua_glue::Table>()) {
        return rawRoots.as<lua_glue::Table>();
    }
    lua_glue::Table roots = lua.create_table();
    fields.raw_set(NATIVE_CONSTRUCTING_ROOTS_FIELD, roots);
    return roots;
}

void beginNativeRootConstruction(lua_glue::StateView lua,
                                 lua_glue::Table fields,
                                 const lua_glue::Table& root) {
    lua_glue::Table roots = nativeRootsUnderConstruction(lua, fields);
    const lua_glue::Object active = roots.raw_get<lua_glue::Object>(root);
    if (active.is<bool>() && active.as<bool>()) {
        throw std::runtime_error("Recursive native root construction for " +
                                 nativeTypeName(lua, root));
    }
    roots.raw_set(root, true);
}

void endNativeRootConstruction(lua_glue::Table fields,
                               const lua_glue::Table& root) {
    const lua_glue::Object rawRoots =
        fields.raw_get<lua_glue::Object>(NATIVE_CONSTRUCTING_ROOTS_FIELD);
    if (!rawRoots.is<lua_glue::Table>()) {
        return;
    }
    lua_glue::Table roots = rawRoots.as<lua_glue::Table>();
    roots.raw_set(root, lua_glue::nil);
    if (tableIsEmpty(roots)) {
        fields.raw_set(NATIVE_CONSTRUCTING_ROOTS_FIELD, lua_glue::nil);
    }
}

lua_glue::Object constructNativeRoot(lua_glue::StateView lua,
                                     const lua_glue::Table& classTable,
                                     const lua_glue::Object& instance,
                                     const lua_glue::Table& root,
                                     const lua_glue::Object& arguments) {
    lua_glue::Table fields = class_native::getUserFields(lua, instance, false);
    const lua_glue::Object rawObjects =
        fields.raw_get<lua_glue::Object>(protocol::NATIVE_OBJECTS_FIELD);
    const lua_glue::Object rawInstanceId =
        fields.raw_get<lua_glue::Object>(INSTANCE_ID_FIELD);
    if (!rawObjects.is<lua_glue::Table>() || !rawInstanceId.is<std::size_t>()) {
        throw std::runtime_error(
            "Composite instance has incomplete native state");
    }
    lua_glue::Table nativeObjects = rawObjects.as<lua_glue::Table>();
    beginNativeRootConstruction(lua, fields, root);
    lua_glue::Object nativeObject = nilObject(lua);
    NativeShadowSnapshot shadowSnapshot;
    try {
        nativeObject = invokeNativeFactory(lua, root, instance, arguments);
        addNativeObject(lua, nativeObjects, root, nativeObject);
        registerNativeOwner(lua, nativeObject, instance);
        syncNativeRootDefaults(lua, classTable, instance, root, nativeObject,
                               shadowSnapshot);
    } catch (...) {
        if ((nativeObject.get_type() == lua_glue::Type::Userdata)) {
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
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        lua_glue::StateView lua(state);
        if (lua_gettop(state) < 1) {
            throw std::invalid_argument(
                "Native base initializer requires a class instance");
        }
        const lua_glue::Table nativeType =
            lua_glue::Read<lua_glue::Table>(state, lua_upvalueindex(1));
        const lua_glue::Object instance =
            lua_glue::Read<lua_glue::Object>(state, 1);
        if (!isCompositeInstance(lua, instance)) {
            throw std::invalid_argument(
                "Native base initializer requires a composite class instance");
        }
        lua_glue::Table fields =
            class_native::getUserFields(lua, instance, false);
        const lua_glue::Object rawInitializing =
            fields.raw_get<lua_glue::Object>(NATIVE_INITIALIZING_FIELD);
        if (!rawInitializing.is<bool>() || !rawInitializing.as<bool>()) {
            throw std::runtime_error(
                "Native base initializers may only run during class "
                "construction");
        }
        const lua_glue::Object rawClass =
            fields.raw_get<lua_glue::Object>(CLASS_FIELD);
        if (!rawClass.is<lua_glue::Table>()) {
            throw std::runtime_error("Composite instance has no class");
        }
        const lua_glue::Table classTable = rawClass.as<lua_glue::Table>();
        bool knownRoot = false;
        for (const lua_glue::Table& root : nativeRoots(lua, classTable)) {
            if (objectsRawEqual(root, nativeType)) {
                knownRoot = true;
                break;
            }
        }
        if (!knownRoot) {
            throw std::invalid_argument(
                "Native initializer target is not an exact class root");
        }
        const lua_glue::Object rawInitialized =
            fields.raw_get<lua_glue::Object>(CLASS_INITIALIZED_ROOTS_FIELD);
        lua_glue::Table initialized = rawInitialized.is<lua_glue::Table>()
                                          ? rawInitialized.as<lua_glue::Table>()
                                          : lua.create_table();
        if (!rawInitialized.is<lua_glue::Table>()) {
            fields.raw_set(CLASS_INITIALIZED_ROOTS_FIELD, initialized);
        }
        const lua_glue::Object alreadyInitialized =
            initialized.raw_get<lua_glue::Object>(nativeType);
        if (alreadyInitialized.is<bool>() && alreadyInitialized.as<bool>()) {
            throw std::runtime_error(
                "Native base initializer was called twice for " +
                nativeTypeName(lua, nativeType));
        }
        const lua_glue::Object rawObjects =
            fields.raw_get<lua_glue::Object>(protocol::NATIVE_OBJECTS_FIELD);
        if (!rawObjects.is<lua_glue::Table>()) {
            throw std::runtime_error(
                "Composite instance has no native object map");
        }
        lua_glue::Table nativeObjects = rawObjects.as<lua_glue::Table>();
        const lua_glue::Object previous =
            nativeObjects.raw_get<lua_glue::Object>(
                nativeTypeName(lua, nativeType));
        const int argumentCount = lua_gettop(state) - 1;
        if ((previous.get_type() == lua_glue::Type::Userdata) &&
            argumentCount == 0) {
            initialized.raw_set(nativeType, true);
            return 0;
        }
        lua_glue::Table arguments = lua.create_table();
        arguments.raw_set("n", argumentCount);
        for (int index = 0; index < argumentCount; ++index) {
            arguments.raw_set(
                index + 1, lua_glue::Read<lua_glue::Object>(state, index + 2));
        }
        if ((previous.get_type() != lua_glue::Type::Userdata)) {
            constructNativeRoot(lua, classTable, instance, nativeType,
                                arguments);
            initialized.raw_set(nativeType, true);
            return 0;
        }
        const lua_glue::Object rawInstanceId =
            fields.raw_get<lua_glue::Object>(INSTANCE_ID_FIELD);
        if (!rawInstanceId.is<std::size_t>()) {
            throw std::runtime_error(
                "Composite instance has no native instance id");
        }
        beginNativeRootConstruction(lua, fields, nativeType);
        lua_glue::Object nativeObject = nilObject(lua);
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
    });
}

}  // namespace

void ensureNativeInitializer(lua_glue::StateView lua,
                             lua_glue::Table nativeType) {
    const lua_glue::Object rawFactory =
        nativeType.raw_get<lua_glue::Object>(CLASS_FACTORY_FIELD);
    const lua_glue::Object rawNew = nativeType.raw_get<lua_glue::Object>("new");
    if (!rawFactory.is<lua_glue::Function>() &&
        !rawNew.is<lua_glue::Function>()) {
        return;
    }
    lua_glue::Object initializer =
        nativeType.raw_get<lua_glue::Object>(NATIVE_INITIALIZER_FIELD);
    if (!initializer.is<lua_glue::Function>()) {
        nativeType.push(lua.lua_state());
        lua_pushcclosure(lua.lua_state(), nativeBaseInitializer, 1);
        initializer = lua_glue::Read<lua_glue::Object>(lua.lua_state(), -1);
        lua_pop(lua.lua_state(), 1);
        nativeType.raw_set(NATIVE_INITIALIZER_FIELD, initializer);
    }
    const lua_glue::Object existing =
        nativeType.raw_get<lua_glue::Object>("init");
    if (!existing.valid() || existing.get_type() == lua_glue::Type::Nil) {
        nativeType.raw_set("init", initializer);
    }
}

}  // namespace ludork::standard::class_runtime::detail
