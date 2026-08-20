#include "Detail/ClassNativeInterop.hpp"
#include "Detail/RuntimeBridge.hpp"

#include <ClassServices.hpp>
#include <LuaError.hpp>

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <climits>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ludork::standard::class_runtime::detail {

using ludork::standard::class_native::getUserFields;

namespace {

sol::object nilObject(sol::state_view lua) {
    return sol::make_object(lua, sol::lua_nil);
}

sol::object protectedIndex(sol::state_view lua, const sol::object& target,
                           const sol::object& key) {
    return class_runtime::protectedGet(lua, target, key);
}

void protectedAssign(sol::state_view lua, const sol::object& target,
                     const sol::object& key, const sol::object& value) {
    class_runtime::protectedSet(lua, target, key, value);
}

sol::table getMro(sol::state_view lua, const sol::table& classTable) {
    return resolverMro(lua, classTable);
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

sol::object classType(sol::this_state state, const sol::object& value) {
    return class_runtime::typeOf(sol::state_view(state), value);
}

bool isSubclass(sol::this_state state, const sol::table& value,
                const sol::table& targetClass) {
    return class_runtime::isSubclassOf(sol::state_view(state), value,
                                       targetClass);
}

bool isInstance(sol::this_state state, const sol::object& value,
                const sol::table& targetClass) {
    return class_runtime::isInstanceOf(sol::state_view(state), value,
                                       targetClass);
}

bool objectsRawEqual(const sol::object& left, const sol::object& right) {
    return class_runtime::rawEqual(left, right);
}

sol::object clonePlainDataImpl(sol::state_view lua, const sol::object& value) {
    return class_runtime::clonePlainData(lua, value);
}

}  // namespace

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
                                    const sol::table& arguments,
                                    std::size_t index) {
    const sol::object value = arguments.raw_get<sol::object>(index);
    return value.valid() ? value : nilObject(lua);
}

struct RuntimeArguments {
    RuntimeArguments(lua_State* value, int firstIndex, int valueCount)
        : state(value),
          first(valueCount == 0 ? 0 : lua_absindex(value, firstIndex)),
          count(valueCount) {}

    sol::object get(sol::state_view lua, std::size_t index) const {
        if (index == 0 || index > static_cast<std::size_t>(count)) {
            return nilObject(lua);
        }
        return sol::stack::get<sol::object>(
            state, first + static_cast<int>(index) - 1);
    }

    lua_State* state = nullptr;
    int first = 0;
    int count = 0;
};

void registerRuntimeService(sol::this_state state, const std::string& name,
                            const sol::protected_function& callback) {
    ludork::standard::class_runtime::registerService(sol::state_view(state),
                                                     name, callback);
}

void unregisterRuntimeService(sol::this_state state, const std::string& name) {
    ludork::standard::class_runtime::unregisterService(sol::state_view(state),
                                                       name);
}

int callRuntimeService(lua_State* state, const std::string& operation,
                       int argumentCount) {
    return ludork::standard::class_runtime::callService(state, operation,
                                                        argumentCount);
}

std::vector<sol::table> runtimeClassMro(sol::state_view lua,
                                        const sol::table& classTable) {
    std::vector<sol::table> result;
    const sol::table mro = getMro(lua, classTable);
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

std::string runtimeValueKind(const sol::object& value) {
    switch (value.get_type()) {
        case sol::type::none:
        case sol::type::lua_nil:
            return "nil";
        case sol::type::boolean:
            return "boolean";
        case sol::type::number:
            return "number";
        case sol::type::string:
            return "string";
        case sol::type::table:
            return "table";
        case sol::type::function:
            return "function";
        case sol::type::userdata:
        case sol::type::lightuserdata:
            return "userdata";
        case sol::type::thread:
            return "thread";
        default:
            return "nil";
    }
}

sol::object runtimeIndex(sol::state_view lua, const sol::object& target,
                         const sol::object& key, bool raw) {
    if (raw && target.get_type() == sol::type::userdata) {
        return getUserFields(lua, target, false).raw_get<sol::object>(key);
    }
    if (raw && !target.is<sol::table>()) {
        return nilObject(lua);
    }
    if (!raw) {
        return protectedIndex(lua, target, key);
    }
    lua_State* state = lua.lua_state();
    target.push();
    key.push();
    lua_rawget(state, -2);
    sol::object result = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 2);
    return result;
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
        sol::table source;
        if (target.is<sol::table>()) {
            source = target.as<sol::table>();
        } else if (target.get_type() == sol::type::userdata) {
            source = getUserFields(lua, target, false);
        } else {
            return keys;
        }
        for (const auto& entry : source) {
            keys.push_back(entry.first);
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

sol::table runtimeMro(sol::state_view lua, const sol::object& value) {
    sol::table result = lua.create_table();
    if (!value.is<sol::table>()) {
        return result;
    }
    const std::vector<sol::table> mro =
        runtimeClassMro(lua, value.as<sol::table>());
    std::size_t index = 1;
    for (const sol::table& type : mro) {
        result.raw_set(index++, type);
    }
    return result;
}

int invokeRuntimeFunction(sol::state_view lua, const sol::object& rawCallable,
                          const std::vector<sol::object>& arguments,
                          const char* context) {
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    try {
        if (arguments.size() > static_cast<std::size_t>(INT_MAX - 1)) {
            throw std::length_error(std::string(context) + " count overflow");
        }
        ensureRuntimeLuaStack(state, arguments.size() + 1, context);
        rawCallable.push();
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

int callRuntimeMethod(sol::state_view lua, const sol::object& receiver,
                      const sol::object& rawName,
                      const sol::object& rawArguments) {
    if (!rawName.is<std::string>()) {
        throw std::invalid_argument("Runtime method name must be a string");
    }
    const std::string name = rawName.as<std::string>();
    const sol::object rawMethod =
        runtimeIndex(lua, receiver, sol::make_object(lua, name), false);
    if (!rawMethod.is<sol::protected_function>()) {
        throw std::runtime_error("Runtime method is not defined: " + name);
    }
    std::vector<sol::object> arguments;
    if (rawArguments.is<sol::table>()) {
        const sol::table table = rawArguments.as<sol::table>();
        std::size_t count = table.size();
        const sol::object rawCount = table.raw_get<sol::object>("n");
        if (rawCount.is<std::size_t>()) {
            count = rawCount.as<std::size_t>();
        }
        arguments.reserve(count + 1);
        arguments.push_back(receiver);
        for (std::size_t index = 1; index <= count; ++index) {
            arguments.push_back(runtimeResolverArgument(lua, table, index));
        }
    } else {
        arguments.push_back(receiver);
    }
    return invokeRuntimeFunction(lua, rawMethod, arguments,
                                 "runtime method arguments");
}

sol::object constructRuntimeClass(sol::state_view lua,
                                  const sol::object& rawClass,
                                  const RuntimeArguments& arguments) {
    if (!rawClass.is<sol::table>()) {
        throw std::invalid_argument(
            "Runtime class constructor requires a class");
    }
    const sol::object rawConstructor =
        rawClass.as<sol::table>().get<sol::object>("new");
    if (!rawConstructor.is<sol::protected_function>()) {
        throw std::runtime_error("Runtime class has no new constructor");
    }
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    try {
        const int argumentCount = std::max(arguments.count - 1, 0);
        ensureRuntimeLuaStack(state,
                              static_cast<std::size_t>(argumentCount) + 1,
                              "runtime constructor call");
        rawConstructor.push();
        for (int index = 2; index <= arguments.count; ++index) {
            arguments.get(lua, static_cast<std::size_t>(index)).push();
        }
        const int status =
            ludork::standard::protectedLuaCall(state, argumentCount, 1);
        ensureRuntimeLuaStack(state, LUA_MINSTACK,
                              "runtime constructor results");
        if (status != LUA_OK) {
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state, -1));
        }
        sol::object result = sol::stack::get<sol::object>(state, -1);
        lua_settop(state, stackBase);
        return result;
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

int invokeRuntimeCallable(sol::state_view lua, const sol::object& rawCallable,
                          const sol::object& rawArguments) {
    if (!rawCallable.is<sol::protected_function>()) {
        throw std::invalid_argument("Runtime callable must be a function");
    }
    std::vector<sol::object> arguments;
    if (rawArguments.is<sol::table>()) {
        const sol::table packed = rawArguments.as<sol::table>();
        std::size_t count = packed.size();
        const sol::object rawCount = packed.raw_get<sol::object>("n");
        if (rawCount.is<std::size_t>()) {
            count = rawCount.as<std::size_t>();
        }
        arguments.reserve(count);
        for (std::size_t index = 1; index <= count; ++index) {
            arguments.push_back(runtimeResolverArgument(lua, packed, index));
        }
    }
    return invokeRuntimeFunction(lua, rawCallable, arguments,
                                 "runtime callable arguments");
}

int runtimeClassResolverImpl(lua_State* state) {
    sol::state_view lua(state);
    const std::string operation = sol::stack::get<std::string>(state, 1);
    const RuntimeArguments arguments(state, 2,
                                     std::max(lua_gettop(state) - 1, 0));
    const sol::object first = arguments.get(lua, 1);
    const sol::object second = arguments.get(lua, 2);
    if (operation == "reflect.type") {
        return runtimeResolverResult(
            lua, {classType(sol::this_state(state), first)});
    }
    if (operation == "reflect.isSubclass") {
        const bool result =
            first.is<sol::table>() && second.is<sol::table>() &&
            isSubclass(sol::this_state(state), first.as<sol::table>(),
                       second.as<sol::table>());
        return runtimeResolverResult(lua, {sol::make_object(lua, result)});
    }
    if (operation == "reflect.isInstance") {
        const bool result =
            second.is<sol::table>() &&
            isInstance(sol::this_state(state), first, second.as<sol::table>());
        return runtimeResolverResult(lua, {sol::make_object(lua, result)});
    }
    if (operation == "reflect.equal") {
        return runtimeResolverResult(
            lua, {sol::make_object(lua, objectsRawEqual(first, second))});
    }
    if (operation == "reflect.mro") {
        return runtimeResolverResult(
            lua, {sol::make_object(lua, runtimeMro(lua, first))});
    }
    if (operation == "reflect.keys" || operation == "reflect.rawKeys") {
        const std::vector<sol::object> keys =
            runtimeKeys(lua, first, operation == "reflect.rawKeys");
        return runtimeResolverResult(
            lua, {sol::make_object(lua, runtimeStringKeys(lua, keys))});
    }
    if (operation == "reflect.get" || operation == "reflect.rawGet") {
        return runtimeResolverResult(
            lua,
            {runtimeIndex(lua, first, second, operation == "reflect.rawGet")});
    }
    if (operation == "reflect.set") {
        const sol::object value = arguments.get(lua, 3);
        runtimeAssign(lua, first, second, value, false);
        return runtimeResolverResult(lua, {});
    }
    if (operation == "reflect.kind") {
        return runtimeResolverResult(
            lua, {sol::make_object(lua, runtimeValueKind(first))});
    }
    if (operation == "reflect.tostring") {
        const sol::object rawToString =
            lua.globals().raw_get<sol::object>("tostring");
        if (!rawToString.is<sol::protected_function>()) {
            throw std::runtime_error("Lua tostring function is not defined");
        }
        sol::protected_function toString =
            rawToString.as<sol::protected_function>();
        sol::protected_function_result result = toString(first);
        return runtimeResolverResult(lua, {checkedResult(lua, result)});
    }
    if (operation == "reflect.construct" || operation == "class.construct") {
        return runtimeResolverResult(
            lua, {constructRuntimeClass(lua, first, arguments)});
    }
    if (operation == "reflect.call") {
        return callRuntimeMethod(lua, first, second, arguments.get(lua, 3));
    }
    if (operation == "reflect.invoke") {
        return invokeRuntimeCallable(lua, first, second);
    }
    if (operation == "reflect.clone") {
        return runtimeResolverResult(lua, {clonePlainDataImpl(lua, first)});
    }
    lua_remove(state, 1);
    return callRuntimeService(state, operation, arguments.count);
}

int runtimeClassResolver(lua_State* state) {
    try {
        return runtimeClassResolverImpl(state);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    } catch (...) {
        return luaL_error(state, "%s", "Unknown runtime resolver error");
    }
}

sol::table runtimeServiceRegistry(sol::state_view lua) {
    sol::table registry = lua.registry();
    const sol::object value =
        registry.raw_get<sol::object>(RUNTIME_SERVICES_KEY);
    if (value.is<sol::table>()) {
        return value.as<sol::table>();
    }
    sol::table result = lua.create_table();
    registry.raw_set(RUNTIME_SERVICES_KEY, result);
    return result;
}

}  // namespace ludork::standard::class_runtime::detail

namespace ludork::standard::class_runtime {

using detail::ensureRuntimeLuaStack;
using detail::runtimeServiceRegistry;

int invoke(lua_State* state, const sol::object& callable, int argumentCount) {
    if (state == nullptr || argumentCount < 0 ||
        argumentCount > lua_gettop(state)) {
        throw std::invalid_argument("Invalid Lua invocation arguments");
    }
    if (!callable.is<sol::protected_function>()) {
        throw std::invalid_argument("Runtime callable must be a function");
    }
    const int stackBase = lua_gettop(state) - argumentCount;
    ensureRuntimeLuaStack(state, 1, "runtime callable");
    callable.push();
    lua_insert(state, stackBase + 1);
    const int status =
        ludork::standard::protectedLuaCall(state, argumentCount, LUA_MULTRET);
    try {
        ensureRuntimeLuaStack(state, LUA_MINSTACK, "runtime callable results");
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
    if (status != LUA_OK) {
        const std::string message =
            ludork::standard::luaErrorMessage(state, -1);
        lua_settop(state, stackBase);
        throw std::runtime_error(message);
    }
    return lua_gettop(state) - stackBase;
}

void registerService(sol::state_view lua, const std::string& name,
                     const sol::protected_function& callback) {
    if (name.empty()) {
        throw std::invalid_argument("Runtime service name must not be empty");
    }
    runtimeServiceRegistry(lua).raw_set(name, callback);
}

void unregisterService(sol::state_view lua, const std::string& name) {
    runtimeServiceRegistry(lua).raw_set(name, sol::lua_nil);
}

int callService(lua_State* state, const std::string& name, int argumentCount) {
    if (state == nullptr || argumentCount < 0 ||
        argumentCount > lua_gettop(state)) {
        throw std::invalid_argument("Invalid runtime service arguments");
    }
    const int stackBase = lua_gettop(state) - argumentCount;
    sol::state_view lua(state);
    const sol::object rawService =
        runtimeServiceRegistry(lua).raw_get<sol::object>(name);
    if (!rawService.is<sol::protected_function>()) {
        lua_settop(state, stackBase);
        throw std::runtime_error("Runtime service '" + name +
                                 "' is not registered");
    }
    ensureRuntimeLuaStack(state, 1, "runtime service callable");
    rawService.push();
    lua_insert(state, stackBase + 1);
    const int status =
        ludork::standard::protectedLuaCall(state, argumentCount, LUA_MULTRET);
    try {
        ensureRuntimeLuaStack(state, LUA_MINSTACK, "runtime service results");
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
    if (status != LUA_OK) {
        const std::string message =
            ludork::standard::luaErrorMessage(state, -1);
        lua_settop(state, stackBase);
        throw std::runtime_error(message);
    }
    return lua_gettop(state) - stackBase;
}

}  // namespace ludork::standard::class_runtime
