#include "Native/NativeRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Instance/InstanceRuntime.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>

namespace ludork::standard::class_runtime::detail {

bool isNativeInitializer(const sol::table& nativeType,
                         const sol::object& member) {
    const sol::object initializer =
        nativeType.raw_get<sol::object>(NATIVE_INITIALIZER_FIELD);
    return initializer.is<sol::function>() && member.is<sol::function>() &&
           objectsRawEqual(initializer, member);
}

namespace {

sol::object cachedBoundMethod(sol::state_view lua, sol::table proxy,
                              const sol::object& key, const sol::object& method,
                              const sol::object& receiver) {
    sol::object rawCache = proxy.raw_get<sol::object>(4);
    sol::table cache = rawCache.is<sol::table>() ? rawCache.as<sol::table>()
                                                 : lua.create_table();
    if (!rawCache.is<sol::table>()) {
        proxy.raw_set(4, cache);
    }
    const sol::object rawEntry = cache.raw_get<sol::object>(key);
    if (rawEntry.is<sol::table>()) {
        const sol::table entry = rawEntry.as<sol::table>();
        const sol::object cachedMethod = entry.raw_get<sol::object>(1);
        const sol::object cachedReceiver = entry.raw_get<sol::object>(2);
        const sol::object cachedWrapper = entry.raw_get<sol::object>(3);
        if (cachedWrapper.is<sol::function>() &&
            objectsRawEqual(cachedMethod, method) &&
            objectsRawEqual(cachedReceiver, receiver)) {
            return cachedWrapper;
        }
    }
    sol::table entry = lua.create_table();
    const sol::object wrapper = bindMethod(lua, method, receiver);
    entry.raw_set(1, method);
    entry.raw_set(2, receiver);
    entry.raw_set(3, wrapper);
    cache.raw_set(key, entry);
    return wrapper;
}

int superProxyIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        const sol::table proxy = sol::stack::get<sol::table>(state, 1);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        const sol::object self = proxy.raw_get<sol::object>(1);
        const sol::table actualClass = proxy.raw_get<sol::table>(2);
        const std::size_t currentIndex = proxy.raw_get<std::size_t>(3);
        const sol::table mro = getMro(lua, actualClass);
        for (std::size_t index = currentIndex + 1; index <= mro.size();
             ++index) {
            const sol::object rawType = mro.raw_get<sol::object>(index);
            if (!rawType.is<sol::table>()) {
                continue;
            }
            const sol::table type = rawType.as<sol::table>();
            const sol::object rawGetters =
                type.raw_get<sol::object>("__getters");
            if (rawGetters.is<sol::table>()) {
                const sol::object getter =
                    rawGetters.as<sol::table>().raw_get<sol::object>(key);
                if (getter.is<sol::function>()) {
                    getter.push();
                    self.push();
                    lua_call(state, 1, 1);
                    return 1;
                }
            }
            sol::object member = nilObject(lua);
            if (isNativeType(lua, type)) {
                const sol::object rawBaseMethods =
                    type.raw_get<sol::object>("__classBaseMethods");
                if (rawBaseMethods.is<sol::table>()) {
                    member =
                        rawBaseMethods.as<sol::table>().raw_get<sol::object>(
                            key);
                }
            }
            if (!member.valid() || member.get_type() == sol::type::lua_nil) {
                member = rawMember(lua, type, key);
            }
            if (!member.valid() || member.get_type() == sol::type::lua_nil) {
                continue;
            }
            if (!member.is<sol::function>()) {
                member.push();
                return 1;
            }
            sol::object receiver = self;
            if (isNativeType(lua, type) && !isNativeInitializer(type, member)) {
                const sol::table fields =
                    class_native::getUserFields(lua, self, false);
                sol::object nativeObject =
                    nativeObjectForType(lua, fields, type);
                if (!nativeObject.is<sol::userdata>()) {
                    nativeObject = ensureDefaultNativeObject(lua, self, type);
                }
                if (nativeObject.is<sol::userdata>()) {
                    receiver = nativeObject;
                }
            }
            cachedBoundMethod(lua, proxy, key, member, receiver).push();
            return 1;
        }
        lua_pushnil(state);
        return 1;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

sol::table superProxyMetatable(sol::state_view lua) {
    sol::table registry = lua.registry();
    const sol::object rawMetatable =
        registry.raw_get<sol::object>(SUPER_PROXY_METATABLE_KEY);
    if (rawMetatable.is<sol::table>()) {
        return rawMetatable.as<sol::table>();
    }
    sol::table metatable = lua.create_table();
    metatable.push();
    lua_pushcfunction(lua.lua_state(), superProxyIndex);
    lua_setfield(lua.lua_state(), -2, "__index");
    lua_pop(lua.lua_state(), 1);
    registry.raw_set(SUPER_PROXY_METATABLE_KEY, metatable);
    return metatable;
}

sol::table createSuperProxy(sol::state_view lua, const sol::table& currentClass,
                            const sol::object& self) {
    const sol::object rawActualClass = actualClassOf(lua, self);
    if (!rawActualClass.is<sol::table>()) {
        throw std::invalid_argument("super() requires a class instance");
    }
    const sol::table actualClass = rawActualClass.as<sol::table>();
    const sol::table mro = getMro(lua, actualClass);
    std::size_t currentIndex = 0;
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (rawType.is<sol::table>() &&
            objectsRawEqual(rawType.as<sol::table>(), currentClass)) {
            currentIndex = index;
            break;
        }
    }
    if (currentIndex == 0) {
        throw std::invalid_argument(
            "super() current class is not in the instance MRO");
    }
    sol::table cache = registryTable(lua, SUPER_PROXY_CACHE_KEY, "k");
    const sol::object rawInstanceCache = cache.raw_get<sol::object>(self);
    sol::table instanceCache = rawInstanceCache.is<sol::table>()
                                   ? rawInstanceCache.as<sol::table>()
                                   : createWeakTable(lua, "v");
    if (!rawInstanceCache.is<sol::table>()) {
        cache.raw_set(self, instanceCache);
    }
    const sol::object rawProxy =
        instanceCache.raw_get<sol::object>(currentClass);
    if (rawProxy.is<sol::table>()) {
        return rawProxy.as<sol::table>();
    }
    sol::table proxy = lua.create_table();
    proxy.raw_set(1, self);
    proxy.raw_set(2, actualClass);
    proxy.raw_set(3, currentIndex);
    proxy[sol::metatable_key] = superProxyMetatable(lua);
    instanceCache.raw_set(currentClass, proxy);
    return proxy;
}

bool inferSuperContext(lua_State* state, sol::table& currentClass,
                       sol::object& self) {
    lua_Debug record{};
    if (lua_getstack(state, 1, &record) == 0 ||
        lua_getinfo(state, "f", &record) == 0) {
        return false;
    }
    sol::state_view lua(state);
    const sol::object caller = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    const sol::object rawOwner =
        registryTable(lua, METHOD_OWNERS_KEY, "k").raw_get<sol::object>(caller);
    if (!rawOwner.is<sol::table>()) {
        return false;
    }
    currentClass = rawOwner.as<sol::table>();
    if (lua_getlocal(state, &record, 1) == nullptr) {
        return false;
    }
    self = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    return true;
}

}  // namespace

int superFunction(lua_State* state) {
    sol::state_view lua(state);
    const int argumentCount = lua_gettop(state);
    if (argumentCount == 2) {
        if (!lua_istable(state, 1)) {
            return luaL_error(state, "super() first argument must be a class");
        }
        createSuperProxy(lua, sol::stack::get<sol::table>(state, 1),
                         sol::stack::get<sol::object>(state, 2))
            .push();
        return 1;
    }
    if (argumentCount != 0 && argumentCount != 1) {
        return luaL_error(state, "super() expects zero, one, or two arguments");
    }
    sol::table currentClass = lua.create_table();
    sol::object inferredSelf = nilObject(lua);
    if (!inferSuperContext(state, currentClass, inferredSelf)) {
        return luaL_error(state,
                          "super() could not determine the defining class");
    }
    const sol::object self = argumentCount == 1
                                 ? sol::stack::get<sol::object>(state, 1)
                                 : inferredSelf;
    createSuperProxy(lua, currentClass, self).push();
    return 1;
}

}  // namespace ludork::standard::class_runtime::detail
