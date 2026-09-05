#include <Runtime/RuntimeReflection.hpp>

#include "RuntimeBindingTraits.hpp"
#include "Runtime/RuntimeSession.hpp"
#include "RuntimeServiceInternals.hpp"
#include <Runtime/Detail/RuntimeServices.hpp>

#include <ClassServices.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>

#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace {

sol::object writeValue(sol::state_view lua, const RuntimeValue& value) {
    return ludork::runtime::binding::writeLuaValue(lua, value);
}

RuntimeValue readValue(const sol::object& value) {
    return ludork::runtime::binding::readLuaValue<RuntimeValue>(value);
}

RuntimeValue::Array collectResults(lua_State* state, int stackBase,
                                   int resultCount) {
    RuntimeValue::Array values;
    values.reserve(static_cast<std::size_t>(resultCount));
    for (int index = 0; index < resultCount; ++index) {
        values.push_back(readValue(
            sol::stack::get<sol::object>(state, stackBase + index + 1)));
    }
    return values;
}

RuntimeValue::Array invokeAndRead(sol::state_view lua,
                                  const sol::object& callable,
                                  const std::vector<sol::object>& arguments,
                                  const char* context) {
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    try {
        const int resultCount = ludork::runtime::detail::invokeRuntimeFunction(
            state, callable, arguments, context);
        RuntimeValue::Array result =
            collectResults(state, stackBase, resultCount);
        lua_settop(state, stackBase);
        return result;
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

std::vector<sol::object> writeArguments(sol::state_view lua,
                                        const RuntimeValue::Array& arguments) {
    std::vector<sol::object> result;
    result.reserve(arguments.size());
    for (const RuntimeValue& argument : arguments) {
        result.push_back(writeValue(lua, argument));
    }
    return result;
}

std::string valueKind(const sol::object& value) {
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

}  // namespace

std::string RuntimeReflectionFacade::kind(const RuntimeValue& value) const {
    ludork::runtime::RuntimeScope runtime;
    return valueKind(writeValue(sol::state_view(runtime.state()), value));
}

RuntimeValue RuntimeReflectionFacade::typeOf(const RuntimeValue& value) const {
    ludork::runtime::RuntimeScope runtime;
    return readValue(ludork::standard::class_runtime::typeOf(
        sol::state_view(runtime.state()),
        writeValue(sol::state_view(runtime.state()), value)));
}

bool RuntimeReflectionFacade::isSubclass(
    const RuntimeValue& value, const RuntimeValue& targetClass) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object rawValue = writeValue(lua, value);
    const sol::object rawTarget = writeValue(lua, targetClass);
    return rawValue.is<sol::table>() && rawTarget.is<sol::table>() &&
           ludork::standard::class_runtime::isSubclassOf(
               lua, rawValue.as<sol::table>(), rawTarget.as<sol::table>());
}

bool RuntimeReflectionFacade::isInstance(
    const RuntimeValue& value, const RuntimeValue& targetClass) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object rawTarget = writeValue(lua, targetClass);
    return rawTarget.is<sol::table>() &&
           ludork::standard::class_runtime::isInstanceOf(
               lua, writeValue(lua, value), rawTarget.as<sol::table>());
}

bool RuntimeReflectionFacade::equal(const RuntimeValue& left,
                                    const RuntimeValue& right) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    return ludork::standard::class_runtime::rawEqual(writeValue(lua, left),
                                                     writeValue(lua, right));
}

RuntimeValue::Array RuntimeReflectionFacade::mro(
    const RuntimeHandle& classType) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object rawType = writeValue(lua, classType);
    if (!rawType.is<sol::table>()) {
        return {};
    }
    const sol::table values =
        ludork::standard::class_runtime::getMroCopy(lua, rawType);
    RuntimeValue::Array result;
    result.reserve(values.size());
    for (std::size_t index = 1; index <= values.size(); ++index) {
        result.push_back(readValue(values.raw_get<sol::object>(index)));
    }
    return result;
}

std::vector<std::string> RuntimeReflectionFacade::keys(
    const RuntimeHandle& value, RuntimeLookupMode mode) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const std::vector<sol::object> rawKeys =
        ludork::runtime::detail::runtimeKeys(lua, writeValue(lua, value),
                                             mode == RuntimeLookupMode::Own);
    std::vector<std::string> result;
    result.reserve(rawKeys.size());
    for (const sol::object& key : rawKeys) {
        if (key.is<std::string>()) {
            result.push_back(key.as<std::string>());
        }
    }
    return result;
}

RuntimeValue RuntimeReflectionFacade::get(const RuntimeHandle& value,
                                          const std::string& name,
                                          RuntimeLookupMode mode) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    return readValue(ludork::runtime::detail::runtimeIndex(
        lua, writeValue(lua, value), sol::make_object(lua, name),
        mode == RuntimeLookupMode::Own));
}

void RuntimeReflectionFacade::set(const RuntimeHandle& value,
                                  const std::string& name,
                                  const RuntimeValue& member) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    ludork::runtime::detail::runtimeAssign(lua, writeValue(lua, value),
                                           sol::make_object(lua, name),
                                           writeValue(lua, member), false);
}

std::string RuntimeReflectionFacade::toString(const RuntimeValue& value) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object rawToString =
        lua.globals().raw_get<sol::object>("tostring");
    if (!rawToString.is<sol::protected_function>()) {
        throw std::runtime_error("Lua tostring function is not defined");
    }
    const RuntimeValue::Array result = invokeAndRead(
        lua, rawToString, {writeValue(lua, value)}, "Lua tostring arguments");
    if (result.size() != 1 || result.front().getIf<std::string>() == nullptr) {
        throw std::runtime_error("Lua tostring must return exactly one string");
    }
    return *result.front().getIf<std::string>();
}

RuntimeValue RuntimeReflectionFacade::construct(
    const RuntimeHandle& classType,
    const RuntimeValue::Array& arguments) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object rawClass = writeValue(lua, classType);
    if (!rawClass.is<sol::table>()) {
        throw std::invalid_argument(
            "Runtime class constructor requires a class");
    }
    const sol::object constructor =
        ludork::standard::class_runtime::protectedGet(
            lua, rawClass, sol::make_object(lua, "new"));
    if (!constructor.is<sol::protected_function>()) {
        throw std::runtime_error("Runtime class has no new constructor");
    }
    RuntimeValue::Array results =
        invokeAndRead(lua, constructor, writeArguments(lua, arguments),
                      "runtime constructor arguments");
    if (results.size() != 1) {
        throw std::runtime_error(
            "Runtime class constructor must return exactly one value");
    }
    return std::move(results.front());
}

RuntimeValue::Array RuntimeReflectionFacade::call(
    const RuntimeHandle& receiver, const std::string& name,
    const RuntimeValue::Array& arguments) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object rawReceiver = writeValue(lua, receiver);
    const sol::object callable = ludork::standard::class_runtime::protectedGet(
        lua, rawReceiver, sol::make_object(lua, name));
    if (!callable.is<sol::protected_function>()) {
        throw std::runtime_error("Runtime method is not defined: " + name);
    }
    std::vector<sol::object> rawArguments;
    rawArguments.reserve(arguments.size() + 1);
    rawArguments.push_back(rawReceiver);
    for (const RuntimeValue& argument : arguments) {
        rawArguments.push_back(writeValue(lua, argument));
    }
    return invokeAndRead(lua, callable, rawArguments,
                         "runtime method arguments");
}

RuntimeValue::Array RuntimeReflectionFacade::invoke(
    const RuntimeHandle& callable, const RuntimeValue::Array& arguments) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object rawCallable = writeValue(lua, callable);
    if (!rawCallable.is<sol::protected_function>()) {
        throw std::invalid_argument("Runtime callable must be a function");
    }
    return invokeAndRead(lua, rawCallable, writeArguments(lua, arguments),
                         "runtime callable arguments");
}

RuntimeValue RuntimeReflectionFacade::clone(const RuntimeValue& value) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    return readValue(
        ludork::standard::class_runtime::deepCopy(lua, writeValue(lua, value)));
}

RuntimeReflectionFacade& runtimeReflection() {
    static RuntimeReflectionFacade reflection;
    return reflection;
}
