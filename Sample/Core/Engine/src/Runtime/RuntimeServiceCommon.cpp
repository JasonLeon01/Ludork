#include "RuntimeServiceInternals.hpp"

#include <ClassServices.hpp>
#include <LuaError.hpp>

extern "C" {
#include <lua.h>
}

#include <climits>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::engine::runtime_detail {

constexpr const char* COMPONENT_CACHES_KEY = "Ludork.Engine.componentCaches";

void ensureRuntimeLuaStack(lua_State* state, std::size_t count,
                           const char* context) {
    if (count > static_cast<std::size_t>(INT_MAX)) {
        throw std::length_error(std::string(context) + " count overflow");
    }
    if (count != 0 && lua_checkstack(state, static_cast<int>(count)) == 0) {
        throw std::runtime_error(std::string("Lua stack cannot grow for ") +
                                 context);
    }
}

int invokeRuntimeFunction(lua_State* state, const sol::object& callable,
                          const std::vector<sol::object>& arguments,
                          const char* context) {
    const int stackBase = lua_gettop(state);
    try {
        if (arguments.size() > static_cast<std::size_t>(INT_MAX - 1)) {
            throw std::length_error(std::string(context) + " count overflow");
        }
        ensureRuntimeLuaStack(state, arguments.size() + 1, context);
        callable.push();
        for (const sol::object& argument : arguments) {
            argument.push();
        }
        const int status = ludork::standard::protectedLuaCall(
            state, static_cast<int>(arguments.size()), LUA_MULTRET);
        ensureRuntimeLuaStack(state, LUA_MINSTACK, "runtime function results");
        if (status != LUA_OK) {
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state, -1));
        }
        return lua_gettop(state) - stackBase;
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

sol::object nilObject(sol::state_view lua) {
    return sol::make_object(lua, sol::lua_nil);
}

sol::object protectedIndex(sol::state_view lua, const sol::object& target,
                           const sol::object& key) {
    return ludork::standard::class_runtime::protectedGet(lua, target, key);
}

void protectedAssign(sol::state_view lua, const sol::object& target,
                     const sol::object& key, const sol::object& value) {
    ludork::standard::class_runtime::protectedSet(lua, target, key, value);
}

sol::table createWeakTable(sol::state_view lua, const char* mode) {
    sol::table result = lua.create_table();
    sol::table metatable = lua.create_table();
    metatable["__mode"] = mode;
    result[sol::metatable_key] = metatable;
    return result;
}

sol::table registryTable(sol::state_view lua, const char* key,
                         const char* weakMode) {
    sol::table registry = lua.registry();
    const sol::object value = registry.raw_get<sol::object>(key);
    if (value.is<sol::table>()) {
        return value.as<sol::table>();
    }
    sol::table result = weakMode == nullptr ? lua.create_table()
                                            : createWeakTable(lua, weakMode);
    registry.raw_set(key, result);
    return result;
}

sol::table componentCache(sol::state_view lua, const sol::object& rawKind) {
    if (!rawKind.is<std::string>()) {
        throw std::invalid_argument("Component cache kind must be a string");
    }
    const std::string kind = rawKind.as<std::string>();
    if (kind != "types" && kind != "fieldDefaults" && kind != "fieldMap" &&
        kind != "inheritedDefaults") {
        throw std::invalid_argument("Unknown component cache kind: " + kind);
    }
    sol::table caches = registryTable(lua, COMPONENT_CACHES_KEY);
    const sol::object rawCache = caches.raw_get<sol::object>(kind);
    if (rawCache.is<sol::table>()) {
        return rawCache.as<sol::table>();
    }
    sol::table cache = createWeakTable(lua, "k");
    caches.raw_set(kind, cache);
    return cache;
}

bool rawBool(const sol::table& table, const char* name) {
    const sol::object value = table.raw_get<sol::object>(name);
    return value.is<bool>() && value.as<bool>();
}

bool isClass(const sol::table& value) {
    const sol::object marker = value.raw_get<sol::object>("__ludorkClass");
    return marker.is<bool>() && marker.as<bool>();
}

sol::table objectMetatable(sol::state_view lua, const sol::object& value) {
    lua_State* state = lua.lua_state();
    value.push();
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        return lua.create_table();
    }
    sol::table result = sol::stack::get<sol::table>(state, -1);
    lua_pop(state, 2);
    return result;
}

bool isNativeType(sol::state_view lua, const sol::table& value) {
    return !isClass(value) && objectMetatable(lua, sol::make_object(lua, value))
                                  .raw_get<sol::object>("__type")
                                  .is<sol::table>();
}

bool isInstance(sol::this_state state, const sol::object& value,
                const sol::table& targetClass) {
    return ludork::standard::class_runtime::isInstanceOf(sol::state_view(state),
                                                         value, targetClass);
}

bool isSubclass(sol::this_state state, const sol::table& value,
                const sol::table& targetClass) {
    return ludork::standard::class_runtime::isSubclassOf(sol::state_view(state),
                                                         value, targetClass);
}

sol::object classType(sol::this_state state, const sol::object& value) {
    return ludork::standard::class_runtime::typeOf(sol::state_view(state),
                                                   value);
}

bool rawEqual(sol::state_view lua, const sol::object& left,
              const sol::object& right) {
    static_cast<void>(lua);
    return ludork::standard::class_runtime::rawEqual(left, right);
}

sol::object checkedResult(sol::state_view lua,
                          sol::protected_function_result& result) {
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(error.what());
    }
    return result.return_count() == 0 ? nilObject(lua)
                                      : result.get<sol::object>();
}

sol::table requireLuaTable(sol::state_view lua, const char* moduleName) {
    const sol::object loaded =
        ludork::standard::class_runtime::requireModule(lua, moduleName);
    if (!loaded.is<sol::table>()) {
        throw std::runtime_error(
            std::string("Lua module did not return a table: ") + moduleName);
    }
    return loaded.as<sol::table>();
}

bool luaBoolean(const sol::object& value) {
    return value.is<bool>() && value.as<bool>();
}

int runtimeResolverResult(sol::state_view lua,
                          const std::vector<sol::object>& values) {
    ensureRuntimeLuaStack(lua.lua_state(), values.size(),
                          "runtime resolver results");
    for (const sol::object& value : values) {
        if (!value.valid() || value.get_type() == sol::type::none) {
            lua_pushnil(lua.lua_state());
        } else {
            value.push();
        }
    }
    return static_cast<int>(values.size());
}

sol::object runtimeResolverArgument(sol::state_view lua,
                                    const RuntimeArguments& arguments,
                                    std::size_t index) {
    if (index == 0 || index > arguments.size()) {
        return nilObject(lua);
    }
    return sol::stack::get<sol::object>(
        arguments.state, arguments.first + static_cast<int>(index) - 1);
}

sol::object callRegisteredRuntimeServiceFirst(
    sol::state_view lua, const std::string& name,
    const std::vector<sol::object>& arguments) {
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    try {
        ensureRuntimeLuaStack(state, arguments.size(),
                              "runtime service arguments");
        for (const sol::object& argument : arguments) {
            argument.push();
        }
        const int resultCount = ludork::standard::class_runtime::callService(
            state, name, static_cast<int>(arguments.size()));
        sol::object result = resultCount == 0 ? nilObject(lua)
                                              : sol::stack::get<sol::object>(
                                                    state, stackBase + 1);
        lua_settop(state, stackBase);
        return result;
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

std::vector<sol::table> runtimeClassMro(sol::state_view lua,
                                        const sol::table& classTable) {
    std::vector<sol::table> result;
    const sol::table mro = ludork::standard::class_runtime::getMroCopy(
        lua, sol::make_object(lua, classTable));
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object value = mro.raw_get<sol::object>(index);
        if (value.is<sol::table>()) {
            result.push_back(value.as<sol::table>());
        }
    }
    if (result.empty()) {
        result.push_back(classTable);
    }
    return result;
}

sol::object runtimeIndex(sol::state_view lua, const sol::object& target,
                         const sol::object& key, bool raw) {
    if (raw) {
        return ludork::standard::class_runtime::rawGetOwnField(lua, target,
                                                               key);
    }
    if (!raw) {
        return protectedIndex(lua, target, key);
    }
    return nilObject(lua);
}

void runtimeAssign(sol::state_view lua, const sol::object& target,
                   const sol::object& key, const sol::object& value, bool raw) {
    if (raw && !target.is<sol::table>()) {
        throw std::invalid_argument("Raw assignment requires a table");
    }
    if (!raw) {
        protectedAssign(lua, target, key, value);
        return;
    }
    lua_State* state = lua.lua_state();
    target.push();
    key.push();
    value.push();
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

std::vector<sol::object> runtimeKeys(sol::state_view lua,
                                     const sol::object& target, bool raw) {
    std::vector<sol::object> keys;
    if (raw) {
        const sol::table ownKeys =
            ludork::standard::class_runtime::getOwnKeys(lua, target);
        keys.reserve(ownKeys.size());
        for (std::size_t index = 1; index <= ownKeys.size(); ++index) {
            keys.push_back(ownKeys.raw_get<sol::object>(index));
        }
        return keys;
    }

    const sol::object rawPairs = lua.globals().raw_get<sol::object>("pairs");
    if (!rawPairs.is<sol::protected_function>()) {
        throw std::runtime_error("Lua pairs function is not defined");
    }
    sol::protected_function pairs = rawPairs.as<sol::protected_function>();
    sol::protected_function_result initialized = pairs(target);
    if (!initialized.valid()) {
        const sol::error error = initialized;
        throw std::runtime_error(error.what());
    }
    if (initialized.return_count() < 3) {
        return keys;
    }
    const sol::object rawIterator = initialized.get<sol::object>(0);
    if (!rawIterator.is<sol::protected_function>()) {
        return keys;
    }
    sol::protected_function iterator =
        rawIterator.as<sol::protected_function>();
    const sol::object iteratorState = initialized.get<sol::object>(1);
    sol::object control = initialized.get<sol::object>(2);
    for (;;) {
        sol::protected_function_result next = iterator(iteratorState, control);
        if (!next.valid()) {
            const sol::error error = next;
            throw std::runtime_error(error.what());
        }
        if (next.return_count() == 0) {
            break;
        }
        sol::object key = next.get<sol::object>(0);
        if (!key.valid() || key.get_type() == sol::type::lua_nil) {
            break;
        }
        keys.push_back(key);
        control = key;
    }
    return keys;
}

sol::table runtimeStringKeys(sol::state_view lua,
                             const std::vector<sol::object>& keys) {
    sol::table result = lua.create_table();
    std::size_t index = 1;
    for (const sol::object& key : keys) {
        if (key.is<std::string>()) {
            result.raw_set(index++, key);
        }
    }
    return result;
}

void clearRuntimeCommonCaches(sol::state_view lua) {
    lua.registry().raw_set(COMPONENT_CACHES_KEY, sol::lua_nil);
}

}  // namespace ludork::engine::runtime_detail
