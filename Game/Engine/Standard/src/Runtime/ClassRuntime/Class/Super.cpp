#include <LuaError.hpp>
#include "Class/ClassRuntimeInternals.hpp"
#include <ClassRuntimeProtocol.hpp>

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypeQueries.hpp"
#include "Instance/InstanceRuntime.hpp"
#include "Native/NativeRuntime.hpp"

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>

namespace ludork::standard::class_runtime::detail {

namespace {

lua_glue::Object cachedBoundMethod(lua_glue::StateView lua,
                                   lua_glue::Table proxy,
                                   const lua_glue::Object& key,
                                   const lua_glue::Object& method,
                                   const lua_glue::Object& receiver) {
    lua_glue::Object rawCache = proxy.raw_get<lua_glue::Object>(4);
    lua_glue::Table cache = rawCache.is<lua_glue::Table>()
                                ? rawCache.as<lua_glue::Table>()
                                : lua.create_table();
    if (!rawCache.is<lua_glue::Table>()) {
        proxy.raw_set(4, cache);
    }
    const lua_glue::Object rawEntry = cache.raw_get<lua_glue::Object>(key);
    if (rawEntry.is<lua_glue::Table>()) {
        const lua_glue::Table entry = rawEntry.as<lua_glue::Table>();
        const lua_glue::Object cachedMethod =
            entry.raw_get<lua_glue::Object>(1);
        const lua_glue::Object cachedReceiver =
            entry.raw_get<lua_glue::Object>(2);
        const lua_glue::Object cachedWrapper =
            entry.raw_get<lua_glue::Object>(3);
        if (cachedWrapper.is<lua_glue::Function>() &&
            objectsRawEqual(cachedMethod, method) &&
            objectsRawEqual(cachedReceiver, receiver)) {
            return cachedWrapper;
        }
    }
    lua_glue::Table entry = lua.create_table();
    const lua_glue::Object wrapper = bindMethod(lua, method, receiver);
    entry.raw_set(1, method);
    entry.raw_set(2, receiver);
    entry.raw_set(3, wrapper);
    cache.raw_set(key, entry);
    return wrapper;
}

int superProxyIndex(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        lua_glue::StateView lua(state);
        const lua_glue::Table proxy = lua_glue::Read<lua_glue::Table>(state, 1);
        const lua_glue::Object key = lua_glue::Read<lua_glue::Object>(state, 2);
        const lua_glue::Object self = proxy.raw_get<lua_glue::Object>(1);
        const lua_glue::Table actualClass = proxy.raw_get<lua_glue::Table>(2);
        const std::size_t currentIndex = proxy.raw_get<std::size_t>(3);
        const lua_glue::Table mro = getMro(lua, actualClass);
        for (std::size_t index = currentIndex + 1; index <= mro.size();
             ++index) {
            const lua_glue::Object rawType =
                mro.raw_get<lua_glue::Object>(index);
            if (!rawType.is<lua_glue::Table>()) {
                continue;
            }
            const lua_glue::Table type = rawType.as<lua_glue::Table>();
            const lua_glue::Object rawGetters =
                type.raw_get<lua_glue::Object>(protocol::CLASS_GETTERS_FIELD);
            if (rawGetters.is<lua_glue::Table>()) {
                const lua_glue::Object getter =
                    rawGetters.as<lua_glue::Table>().raw_get<lua_glue::Object>(
                        key);
                if (getter.is<lua_glue::Function>()) {
                    getter.push(state);
                    self.push(state);
                    if (ludork::standard::protectedLuaCall(state, 1, 1) !=
                        LUA_OK) {
                        throw std::runtime_error(
                            ludork::standard::luaErrorMessage(state, -1));
                    }
                    return 1;
                }
            }
            lua_glue::Object member = nilObject(lua);
            if (isNativeType(lua, type)) {
                const lua_glue::Object rawBaseMethods =
                    type.raw_get<lua_glue::Object>(CLASS_BASE_METHODS_FIELD);
                if (rawBaseMethods.is<lua_glue::Table>()) {
                    member = rawBaseMethods.as<lua_glue::Table>()
                                 .raw_get<lua_glue::Object>(key);
                }
            }
            if (!member.valid() || member.get_type() == lua_glue::Type::Nil) {
                member = rawMember(lua, type, key);
            }
            if (!member.valid() || member.get_type() == lua_glue::Type::Nil) {
                continue;
            }
            if (!member.is<lua_glue::Function>()) {
                member.push(state);
                return 1;
            }
            lua_glue::Object receiver = self;
            if (isNativeType(lua, type) && !isNativeInitializer(type, member)) {
                const lua_glue::Table fields =
                    class_native::getUserFields(lua, self, false);
                lua_glue::Object nativeObject =
                    nativeObjectForType(lua, fields, type);
                if ((nativeObject.get_type() != lua_glue::Type::Userdata)) {
                    nativeObject = ensureDefaultNativeObject(lua, self, type);
                }
                if ((nativeObject.get_type() == lua_glue::Type::Userdata)) {
                    receiver = nativeObject;
                }
            }
            cachedBoundMethod(lua, proxy, key, member, receiver).push(state);
            return 1;
        }
        lua_pushnil(state);
        return 1;
    });
}

lua_glue::Table superProxyMetatable(lua_glue::StateView lua) {
    lua_glue::Table registry = lua.registry();
    const lua_glue::Object rawMetatable =
        registry.raw_get<lua_glue::Object>(SUPER_PROXY_METATABLE_KEY);
    if (rawMetatable.is<lua_glue::Table>()) {
        return rawMetatable.as<lua_glue::Table>();
    }
    lua_glue::Table metatable = lua.create_table();
    metatable.push(lua.lua_state());
    lua_pushcfunction(lua.lua_state(), superProxyIndex);
    lua_setfield(lua.lua_state(), -2, "__index");
    lua_pop(lua.lua_state(), 1);
    registry.raw_set(SUPER_PROXY_METATABLE_KEY, metatable);
    return metatable;
}

lua_glue::Table createSuperProxy(lua_glue::StateView lua,
                                 const lua_glue::Table& currentClass,
                                 const lua_glue::Object& self) {
    const lua_glue::Object rawActualClass = actualClassOf(lua, self);
    if (!rawActualClass.is<lua_glue::Table>()) {
        throw std::invalid_argument("super() requires a class instance");
    }
    const lua_glue::Table actualClass = rawActualClass.as<lua_glue::Table>();
    const lua_glue::Table mro = getMro(lua, actualClass);
    std::size_t currentIndex = 0;
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const lua_glue::Object rawType = mro[index];
        if (rawType.is<lua_glue::Table>() &&
            objectsRawEqual(rawType.as<lua_glue::Table>(), currentClass)) {
            currentIndex = index;
            break;
        }
    }
    if (currentIndex == 0) {
        throw std::invalid_argument(
            "super() current class is not in the instance MRO");
    }
    lua_glue::Table cache = registryTable(lua, SUPER_PROXY_CACHE_KEY, "k");
    const lua_glue::Object rawInstanceCache =
        cache.raw_get<lua_glue::Object>(self);
    lua_glue::Table instanceCache = rawInstanceCache.is<lua_glue::Table>()
                                        ? rawInstanceCache.as<lua_glue::Table>()
                                        : createWeakTable(lua, "v");
    if (!rawInstanceCache.is<lua_glue::Table>()) {
        cache.raw_set(self, instanceCache);
    }
    const lua_glue::Object rawProxy =
        instanceCache.raw_get<lua_glue::Object>(currentClass);
    if (rawProxy.is<lua_glue::Table>()) {
        return rawProxy.as<lua_glue::Table>();
    }
    lua_glue::Table proxy = lua.create_table();
    proxy.raw_set(1, self);
    proxy.raw_set(2, actualClass);
    proxy.raw_set(3, currentIndex);
    lua_glue::SetMetatable(proxy, superProxyMetatable(lua));
    instanceCache.raw_set(currentClass, proxy);
    return proxy;
}

bool inferSuperContext(lua_State* state, lua_glue::Table& currentClass,
                       lua_glue::Object& self) {
    lua_Debug record{};
    if (lua_getstack(state, 1, &record) == 0 ||
        lua_getinfo(state, "f", &record) == 0) {
        return false;
    }
    lua_glue::StateView lua(state);
    const lua_glue::Object caller = lua_glue::Read<lua_glue::Object>(state, -1);
    lua_pop(state, 1);
    const lua_glue::Object rawOwner = registryTable(lua, METHOD_OWNERS_KEY, "k")
                                          .raw_get<lua_glue::Object>(caller);
    if (!rawOwner.is<lua_glue::Table>()) {
        return false;
    }
    currentClass = rawOwner.as<lua_glue::Table>();
    if (lua_getlocal(state, &record, 1) == nullptr) {
        return false;
    }
    self = lua_glue::Read<lua_glue::Object>(state, -1);
    lua_pop(state, 1);
    return true;
}

}  // namespace

int superFunction(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        lua_glue::StateView lua(state);
        const int argumentCount = lua_gettop(state);
        if (argumentCount == 2) {
            if (!lua_istable(state, 1)) {
                throw std::invalid_argument(
                    "super() first argument must be a class");
            }
            createSuperProxy(lua, lua_glue::Read<lua_glue::Table>(state, 1),
                             lua_glue::Read<lua_glue::Object>(state, 2))
                .push(state);
            return 1;
        }
        if (argumentCount != 0 && argumentCount != 1) {
            throw std::invalid_argument(
                "super() expects zero, one, or two arguments");
        }
        lua_glue::Table currentClass = lua.create_table();
        lua_glue::Object inferredSelf = nilObject(lua);
        if (!inferSuperContext(state, currentClass, inferredSelf)) {
            throw std::invalid_argument(
                "super() could not determine the defining class");
        }
        const lua_glue::Object self =
            argumentCount == 1 ? lua_glue::Read<lua_glue::Object>(state, 1)
                               : inferredSelf;
        createSuperProxy(lua, currentClass, self).push(state);
        return 1;
    });
}

}  // namespace ludork::standard::class_runtime::detail
