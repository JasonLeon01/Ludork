#include <Runtime/RuntimeReference.hpp>

#include "RuntimeBindingTraits.hpp"
#include <Runtime/Detail/RuntimeServices.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <Runtime/ScriptStore.hpp>
#include <ClassServices.hpp>
#include <LuaError.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>

extern "C" {
#include <lua.h>
}

#include <sol2/sol.hpp>

namespace ludork::runtime::reference {
namespace {

sol::object write(sol::state_view lua, const RuntimeValue& value) {
    return binding::writeLuaValue(lua, value);
}

RuntimeValue read(const sol::object& value) {
    switch (value.get_type()) {
        case sol::type::none:
        case sol::type::lua_nil:
            return {};
        case sol::type::boolean:
        case sol::type::number:
        case sol::type::string:
            return binding::readLuaValue<RuntimeValue>(value);
        default:
            return RuntimeValue(
                binding::readOpaqueIdentity<RuntimeIdentityPtr>(value));
    }
}

RuntimeHandle capture(const sol::object& value) {
    return RuntimeHandle(
        binding::readOpaqueIdentity<RuntimeIdentityPtr>(value));
}

const char* weakMode(WeakMode mode) {
    switch (mode) {
        case WeakMode::None:
            return nullptr;
        case WeakMode::Keys:
            return "k";
        case WeakMode::Values:
            return "v";
        case WeakMode::KeysAndValues:
            return "kv";
    }
    throw std::invalid_argument("Unknown weak reference mode");
}

RuntimeValue::Array collect(lua_State* state, int base, int count) {
    RuntimeValue::Array values;
    values.reserve(static_cast<std::size_t>(count));
    for (int index = 1; index <= count; ++index) {
        values.push_back(
            read(sol::stack::get<sol::object>(state, base + index)));
    }
    return values;
}

}  // namespace

RuntimeHandle intern(const RuntimeValue& value) {
    if (const RuntimeHandle* handle = value.getIf<RuntimeHandle>()) {
        return *handle;
    }
    if (value.isNil()) {
        return {};
    }
    RuntimeScope scope;
    return capture(write(sol::state_view(scope.state()), value));
}

RuntimeData data(const RuntimeValue& value) {
    return value.getIf<RuntimeHandle>() == nullptr ? value.toData()
                                                   : snapshot(value).toData();
}

RuntimeValue retain(const RuntimeValue& value) {
    RuntimeScope scope;
    return read(write(sol::state_view(scope.state()), value));
}

RuntimeValue snapshot(const RuntimeValue& value) {
    RuntimeScope scope;
    return binding::readLuaValue<RuntimeValue>(
        write(sol::state_view(scope.state()), value));
}

RuntimeIdentityPtr identity(const RuntimeValue& value) {
    if (value.isNil()) {
        return nullptr;
    }
    RuntimeScope scope;
    return binding::readOpaqueIdentity<RuntimeIdentityPtr>(
        write(sol::state_view(scope.state()), value));
}

std::shared_ptr<RuntimeObject> object(const RuntimeValue& value) {
    RuntimeScope scope;
    std::shared_ptr<RuntimeObject> result;
    if (!binding::tryReadSharedPointer(
            write(sol::state_view(scope.state()), value), result)) {
        return nullptr;
    }
    return result;
}

RuntimeHandle table(WeakMode mode) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    const char* weak = weakMode(mode);
    return capture(sol::make_object(
        lua, weak == nullptr ? lua.create_table()
                             : detail::createWeakTable(lua, weak)));
}

RuntimeHandle globals() {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    return capture(sol::make_object(lua, lua.globals()));
}

RuntimeHandle registry() {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    return capture(sol::make_object(lua, lua.registry()));
}

RuntimeHandle registryTable(const std::string& key, WeakMode mode) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    return capture(sol::make_object(
        lua, detail::registryTable(lua, key.c_str(), weakMode(mode))));
}

RuntimeValue get(const RuntimeHandle& target, const RuntimeValue& key) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    return read(
        detail::runtimeIndex(lua, write(lua, target), write(lua, key), false));
}

RuntimeValue rawGet(const RuntimeHandle& target, const RuntimeValue& key) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    return read(
        detail::runtimeIndex(lua, write(lua, target), write(lua, key), true));
}

void set(const RuntimeHandle& target, const RuntimeValue& key,
         const RuntimeValue& value) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    detail::runtimeAssign(lua, write(lua, target), write(lua, key),
                          write(lua, value), false);
}

void rawSet(const RuntimeHandle& target, const RuntimeValue& key,
            const RuntimeValue& value) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    detail::runtimeAssign(lua, write(lua, target), write(lua, key),
                          write(lua, value), true);
}

Entries entries(const RuntimeHandle& target) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    const sol::object raw = write(lua, target);
    if (!raw.is<sol::table>()) {
        throw std::invalid_argument("Runtime entries require a table");
    }
    Entries result;
    for (const auto& entry : raw.as<sol::table>()) {
        result.emplace_back(read(entry.first), read(entry.second));
    }
    return result;
}

RuntimeValue::Array keys(const RuntimeHandle& target, RuntimeLookupMode mode) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    RuntimeValue::Array result;
    for (const sol::object& key : detail::runtimeKeys(
             lua, write(lua, target), mode == RuntimeLookupMode::Own)) {
        result.push_back(read(key));
    }
    return result;
}

std::size_t length(const RuntimeHandle& target) {
    RuntimeScope scope;
    const sol::object raw = write(sol::state_view(scope.state()), target);
    if (!raw.is<sol::table>()) {
        throw std::invalid_argument("Runtime length requires a table");
    }
    return raw.as<sol::table>().size();
}

std::string kind(const RuntimeValue& value) {
    return runtimeReflection().kind(value);
}

bool hasMetatable(const RuntimeHandle& value) {
    RuntimeScope scope;
    lua_State* state = scope.state();
    write(sol::state_view(state), value).push();
    const bool present = lua_getmetatable(state, -1) != 0;
    lua_pop(state, present ? 2 : 1);
    return present;
}

RuntimeHandle metatable(const RuntimeHandle& value) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    return capture(
        sol::make_object(lua, detail::objectMetatable(lua, write(lua, value))));
}

void setMetatable(const RuntimeHandle& value,
                  const RuntimeHandle& valueMetatable) {
    RuntimeScope scope;
    lua_State* state = scope.state();
    const sol::object target = write(sol::state_view(state), value);
    const sol::object meta = write(sol::state_view(state), valueMetatable);
    if (!target.is<sol::table>() || !meta.is<sol::table>()) {
        throw std::invalid_argument(
            "Runtime metatable assignment requires tables");
    }
    target.push();
    meta.push();
    lua_setmetatable(state, -2);
    lua_pop(state, 1);
}

bool isCallable(const RuntimeValue& value) {
    if (isFunction(value)) {
        return true;
    }
    const std::string type = kind(value);
    return (type == "table" || type == "userdata") &&
           isFunction(rawGet(
               metatable(ludork::runtime::reference::intern(value)), "__call"));
}

RuntimeValue::Array invoke(const RuntimeHandle& callable,
                           const RuntimeValue::Array& arguments) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    if (!isCallable(callable)) {
        throw std::invalid_argument("Runtime value is not callable");
    }
    std::vector<sol::object> values;
    values.reserve(arguments.size());
    for (const RuntimeValue& argument : arguments) {
        values.push_back(write(lua, argument));
    }
    const int base = lua_gettop(scope.state());
    try {
        const int count = detail::invokeRuntimeFunction(
            scope.state(), write(lua, callable), values,
            "runtime reference arguments");
        RuntimeValue::Array result = collect(scope.state(), base, count);
        lua_settop(scope.state(), base);
        return result;
    } catch (...) {
        lua_settop(scope.state(), base);
        throw;
    }
}

RuntimeHandle callback(Callback function) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    auto wrapper = [function = std::move(function)](
                       sol::this_state state, sol::variadic_args arguments) {
        standard::LuaExecutionScope execution(state);
        if (!execution.active()) {
            throw std::runtime_error("Lua runtime session is stopping");
        }
        RuntimeValue::Array values;
        values.reserve(arguments.size());
        for (const auto& argument : arguments) {
            values.push_back(read(argument.get<sol::object>()));
        }
        const RuntimeValue::Array results = function(values);
        sol::variadic_results output;
        output.reserve(results.size());
        for (const RuntimeValue& result : results) {
            output.push_back(write(sol::state_view(state), result));
        }
        return output;
    };
    return capture(sol::make_object(lua, sol::as_function(std::move(wrapper))));
}

std::vector<std::string> functionParameterNames(const RuntimeValue& method) {
    std::vector<std::string> names;
    if (!isFunction(method)) {
        return names;
    }
    const RuntimeValue rawDebug = rawGet(globals(), "debug");
    if (!isTable(rawDebug)) {
        return names;
    }
    const RuntimeValue debug = rawDebug;
    const RuntimeValue rawGetInfo =
        rawGet(ludork::runtime::reference::intern(debug), "getinfo");
    const RuntimeValue rawGetLocal =
        rawGet(ludork::runtime::reference::intern(debug), "getlocal");
    if (!isFunction(rawGetInfo) || !isFunction(rawGetLocal)) {
        return names;
    }
    RuntimeValue getInfo = rawGetInfo;
    RuntimeValue::Array infoResult =
        invoke(ludork::runtime::reference::intern(getInfo),
               {makeValue(method), makeValue("u")});
    const RuntimeValue rawInfo = first(infoResult);
    if (!isTable(rawInfo)) {
        return names;
    }
    const RuntimeValue rawCount =
        rawGet(ludork::runtime::reference::intern(rawInfo), "nparams");
    const std::size_t count =
        is<std::size_t>(rawCount) ? as<std::size_t>(rawCount) : 0;
    RuntimeValue getLocal = rawGetLocal;
    names.reserve(count);
    for (std::size_t index = 1; index <= count; ++index) {
        RuntimeValue::Array localResult =
            invoke(ludork::runtime::reference::intern(getLocal),
                   {makeValue(method), makeValue(index)});

        if (localResult.size() == 0) {
            continue;
        }
        const RuntimeValue rawName = first(localResult);
        if (!is<std::string>(rawName)) {
            continue;
        }
        const std::string name = as<std::string>(rawName);
        if (name != "self") {
            names.push_back(name);
        }
    }
    return names;
}

std::optional<FunctionSource> functionSource(const RuntimeValue& callable) {
    if (!isFunction(callable)) {
        return std::nullopt;
    }
    const RuntimeValue debug = rawGet(globals(), "debug");
    if (!isTable(debug)) {
        return std::nullopt;
    }
    const RuntimeValue getInfo =
        rawGet(ludork::runtime::reference::intern(debug), "getinfo");
    if (!isFunction(getInfo)) {
        return std::nullopt;
    }
    const RuntimeValue info =
        first(invoke(ludork::runtime::reference::intern(getInfo),
                     {callable, makeValue("S")}));
    if (!isTable(info)) {
        return std::nullopt;
    }
    const RuntimeValue what =
        rawGet(ludork::runtime::reference::intern(info), "what");
    const RuntimeValue source =
        rawGet(ludork::runtime::reference::intern(info), "source");
    const RuntimeValue firstLine =
        rawGet(ludork::runtime::reference::intern(info), "linedefined");
    const RuntimeValue lastLine =
        rawGet(ludork::runtime::reference::intern(info), "lastlinedefined");
    if (!is<std::string>(what) || as<std::string>(what) != "Lua" ||
        !is<std::string>(source) || !as<std::string>(source).starts_with('@') ||
        !is<std::size_t>(firstLine) || !is<std::size_t>(lastLine)) {
        return std::nullopt;
    }
    return FunctionSource{as<std::string>(source).substr(1),
                          as<std::size_t>(firstLine),
                          as<std::size_t>(lastLine)};
}

RuntimeValue deepCopy(const RuntimeValue& value) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    return read(standard::class_runtime::deepCopy(lua, write(lua, value)));
}

RuntimeValue requireModule(const std::string& module) {
    RuntimeScope scope;
    return read(standard::class_runtime::requireModule(
        sol::state_view(scope.state()), module));
}

bool moduleExists(const std::string& module) {
    const RuntimeValue package = rawGet(globals(), "package");
    if (!isTable(package)) {
        return false;
    }
    for (const char* field : {"loaded", "preload"}) {
        const RuntimeValue modules =
            rawGet(ludork::runtime::reference::intern(package), field);
        if (isTable(modules) &&
            !rawGet(ludork::runtime::reference::intern(modules), module)
                 .isNil()) {
            return true;
        }
    }
    const RuntimeValue search =
        rawGet(ludork::runtime::reference::intern(package), "searchpath");
    if (!isFunction(search)) {
        return false;
    }
    for (const char* field : {"path", "cpath"}) {
        const RuntimeValue path =
            rawGet(ludork::runtime::reference::intern(package), field);
        if (!is<std::string>(path)) {
            continue;
        }
        try {
            if (is<std::string>(
                    first(invoke(ludork::runtime::reference::intern(search),
                                 {makeValue(module), path})))) {
                return true;
            }
        } catch (const std::exception&) {}
    }
    return false;
}

RuntimeValue::Array executeScript(const std::string& path) {
    RuntimeScope scope;
    lua_State* state = scope.state();
    const int base = lua_gettop(state);
    try {
        if (scriptStore().loadFile(state, path) != LUA_OK ||
            standard::protectedLuaCall(state, 0, LUA_MULTRET) != LUA_OK) {
            throw std::runtime_error(standard::luaErrorMessage(state, -1));
        }
        RuntimeValue::Array result =
            collect(state, base, lua_gettop(state) - base);
        lua_settop(state, base);
        return result;
    } catch (...) {
        lua_settop(state, base);
        throw;
    }
}

RuntimeHandle finalizeClass(const RuntimeHandle& definition,
                            const RuntimeHandle& bases) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    return capture(
        sol::make_object(lua, standard::class_runtime::finalizeClass(
                                  write(lua, definition).as<sol::table>(),
                                  write(lua, bases).as<sol::table>())));
}

RuntimeValue classType(const RuntimeValue& value) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    return read(standard::class_runtime::typeOf(lua, write(lua, value)));
}

RuntimeValue::Array classMro(const RuntimeHandle& value) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    const sol::table mro =
        standard::class_runtime::getMroCopy(lua, write(lua, value));
    RuntimeValue::Array result;
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        result.push_back(read(mro.raw_get<sol::object>(index)));
    }
    return result;
}

RuntimeValue typeMetadata(const RuntimeHandle& value) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    return read(
        detail::runtimeTypeMetadata(lua, write(lua, value).as<sol::table>()));
}

bool isClass(const RuntimeValue& value) {
    return isTable(value) &&
           is<bool>(rawGet(ludork::runtime::reference::intern(value),
                           "__ludorkClass")) &&
           as<bool>(rawGet(ludork::runtime::reference::intern(value),
                           "__ludorkClass"));
}

bool isNativeType(const RuntimeValue& value) {
    return isTable(value) && !isClass(value) &&
           isTable(rawGet(metatable(ludork::runtime::reference::intern(value)),
                          "__type"));
}

bool isInstance(const RuntimeValue& value, const RuntimeValue& type) {
    return runtimeReflection().isInstance(value, type);
}

bool hasOwnField(const RuntimeHandle& target, const RuntimeValue& key) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    return standard::class_runtime::hasOwnField(lua, write(lua, target),
                                                write(lua, key));
}

bool rawEqual(const RuntimeValue& left, const RuntimeValue& right) {
    return runtimeReflection().equal(left, right);
}

void setNativeDefaultResolver(Callback resolver) {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    const RuntimeHandle function = callback(std::move(resolver));
    standard::class_runtime::registerNativeClassDefaultResolver(
        lua, write(lua, function).as<sol::protected_function>());
}

void clearNativeDefaultResolver(lua_State* state) {
    standard::class_runtime::unregisterNativeClassDefaultResolver(
        sol::state_view(state));
}

}  // namespace ludork::runtime::reference
