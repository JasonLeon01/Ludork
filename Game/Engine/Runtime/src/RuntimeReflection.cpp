#include <Runtime/RuntimeReflection.hpp>

#include "LuaServices/RuntimeBindingTraits.hpp"
#include "Runtime/RuntimeSession.hpp"
#include <Runtime/Detail/RuntimeServices.hpp>

#include <ClassServices.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace {

lua_glue::Object writeValue(lua_glue::StateView lua,
                            const RuntimeValue& value) {
    return ludork::runtime::binding::writeLuaValue(lua, value);
}

RuntimeValue readValue(const lua_glue::Object& value) {
    return ludork::runtime::binding::readLuaValue<RuntimeValue>(value);
}

RuntimeValue::Array collectResults(lua_State* state, int stackBase,
                                   int resultCount) {
    RuntimeValue::Array values;
    values.reserve(static_cast<std::size_t>(resultCount));
    for (int index = 0; index < resultCount; ++index) {
        values.push_back(readValue(
            lua_glue::Read<lua_glue::Object>(state, stackBase + index + 1)));
    }
    return values;
}

RuntimeValue::Array invokeAndRead(
    lua_glue::StateView lua, const lua_glue::Object& callable,
    const std::vector<lua_glue::Object>& arguments, const char* context) {
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

std::vector<lua_glue::Object> writeArguments(
    lua_glue::StateView lua, const RuntimeValue::Array& arguments) {
    std::vector<lua_glue::Object> result;
    result.reserve(arguments.size());
    for (const RuntimeValue& argument : arguments) {
        result.push_back(writeValue(lua, argument));
    }
    return result;
}

std::string valueKind(const lua_glue::Object& value) {
    switch (value.get_type()) {
        case lua_glue::Type::None:
        case lua_glue::Type::Nil:
            return "nil";
        case lua_glue::Type::Boolean:
            return "boolean";
        case lua_glue::Type::Number:
            return "number";
        case lua_glue::Type::String:
            return "string";
        case lua_glue::Type::Table:
            return "table";
        case lua_glue::Type::Function:
            return "function";
        case lua_glue::Type::Userdata:
        case lua_glue::Type::LightUserdata:
            return "userdata";
        case lua_glue::Type::Thread:
            return "thread";
        default:
            return "nil";
    }
}

}  // namespace

std::string RuntimeReflectionFacade::kind(const RuntimeValue& value) const {
    ludork::runtime::RuntimeScope runtime;
    if (value.isNil()) {
        return "nil";
    }
    if (value.getIf<bool>() != nullptr) {
        return "boolean";
    }
    if (value.getIf<std::int64_t>() != nullptr ||
        value.getIf<double>() != nullptr) {
        return "number";
    }
    if (value.getIf<std::string>() != nullptr) {
        return "string";
    }
    if (value.view().array() || value.view().map()) {
        return "table";
    }
    return valueKind(writeValue(lua_glue::StateView(runtime.state()), value));
}

RuntimeValue RuntimeReflectionFacade::typeOf(const RuntimeValue& value) const {
    ludork::runtime::RuntimeScope runtime;
    return readValue(ludork::standard::class_runtime::typeOf(
        lua_glue::StateView(runtime.state()),
        writeValue(lua_glue::StateView(runtime.state()), value)));
}

bool RuntimeReflectionFacade::isSubclass(
    const RuntimeValue& value, const RuntimeValue& targetClass) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const lua_glue::Object rawValue = writeValue(lua, value);
    const lua_glue::Object rawTarget = writeValue(lua, targetClass);
    return rawValue.is<lua_glue::Table>() && rawTarget.is<lua_glue::Table>() &&
           ludork::standard::class_runtime::isSubclassOf(
               lua, rawValue.as<lua_glue::Table>(),
               rawTarget.as<lua_glue::Table>());
}

bool RuntimeReflectionFacade::isInstance(
    const RuntimeValue& value, const RuntimeValue& targetClass) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const lua_glue::Object rawTarget = writeValue(lua, targetClass);
    return rawTarget.is<lua_glue::Table>() &&
           ludork::standard::class_runtime::isInstanceOf(
               lua, writeValue(lua, value), rawTarget.as<lua_glue::Table>());
}

bool RuntimeReflectionFacade::equal(const RuntimeValue& left,
                                    const RuntimeValue& right) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    return ludork::standard::class_runtime::rawEqual(writeValue(lua, left),
                                                     writeValue(lua, right));
}

RuntimeValue::Array RuntimeReflectionFacade::mro(
    const RuntimeHandle& classType) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const lua_glue::Object rawType = writeValue(lua, classType);
    if (!rawType.is<lua_glue::Table>()) {
        return {};
    }
    const lua_glue::Table values =
        ludork::standard::class_runtime::getMroCopy(lua, rawType);
    RuntimeValue::Array result;
    result.reserve(values.size());
    for (std::size_t index = 1; index <= values.size(); ++index) {
        result.push_back(readValue(values.raw_get<lua_glue::Object>(index)));
    }
    return result;
}

std::vector<std::string> RuntimeReflectionFacade::keys(
    const RuntimeHandle& value,
    RuntimeReflectionFacade::RuntimeLookupMode mode) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const std::vector<lua_glue::Object> rawKeys =
        ludork::runtime::detail::runtimeKeys(
            lua, writeValue(lua, value),
            mode == RuntimeReflectionFacade::RuntimeLookupMode::Own);
    std::vector<std::string> result;
    result.reserve(rawKeys.size());
    for (const lua_glue::Object& key : rawKeys) {
        if (key.is<std::string>()) {
            result.push_back(key.as<std::string>());
        }
    }
    return result;
}

RuntimeValue RuntimeReflectionFacade::get(
    const RuntimeHandle& value, const std::string& name,
    RuntimeReflectionFacade::RuntimeLookupMode mode) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    return readValue(ludork::runtime::detail::runtimeIndex(
        lua, writeValue(lua, value), lua_glue::MakeObject(lua, name),
        mode == RuntimeReflectionFacade::RuntimeLookupMode::Own));
}

void RuntimeReflectionFacade::set(const RuntimeHandle& value,
                                  const std::string& name,
                                  const RuntimeValue& member) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    ludork::runtime::detail::runtimeAssign(lua, writeValue(lua, value),
                                           lua_glue::MakeObject(lua, name),
                                           writeValue(lua, member), false);
}

void RuntimeReflectionFacade::setTyped(const RuntimeHandle& value,
                                       const std::string& name,
                                       const RuntimeValue& member) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua(runtime.state());
    ludork::standard::class_runtime::protectedSetTyped(
        lua, writeValue(lua, value), lua_glue::MakeObject(lua, name),
        writeValue(lua, member));
}

RuntimeNumber RuntimeReflectionFacade::getNumber(
    const RuntimeHandle& value, const std::string& name) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua(runtime.state());
    const lua_glue::Object member = ludork::runtime::detail::runtimeIndex(
        lua, writeValue(lua, value), lua_glue::MakeObject(lua, name), false);
    const lua_glue::PushGuard pushed(member);
    lua_State* state = runtime.state();
    if (lua_type(state, pushed.index()) != LUA_TNUMBER) {
        throw std::logic_error("Numeric attribute has an incompatible value: " +
                               name);
    }
    if (lua_isinteger(state, pushed.index())) {
        return static_cast<std::int64_t>(lua_tointeger(state, pushed.index()));
    }
    return static_cast<double>(lua_tonumber(state, pushed.index()));
}

void RuntimeReflectionFacade::setNumber(const RuntimeHandle& value,
                                        const std::string& name,
                                        const RuntimeNumber& member) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua(runtime.state());
    const lua_glue::Object rawMember = std::visit(
        [lua](auto number) {
            return lua_glue::MakeObject(lua, number);
        },
        member);
    ludork::runtime::detail::runtimeAssign(lua, writeValue(lua, value),
                                           lua_glue::MakeObject(lua, name),
                                           rawMember, false);
}

std::string RuntimeReflectionFacade::toString(const RuntimeValue& value) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const lua_glue::Object rawToString =
        lua.globals().raw_get<lua_glue::Object>("tostring");
    if (!rawToString.is<lua_glue::Function>()) {
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
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const lua_glue::Object rawClass = writeValue(lua, classType);
    if (!rawClass.is<lua_glue::Table>()) {
        throw std::invalid_argument(
            "Runtime class constructor requires a class");
    }
    const lua_glue::Object constructor =
        ludork::standard::class_runtime::protectedGet(
            lua, rawClass, lua_glue::MakeObject(lua, "new"));
    if (!constructor.is<lua_glue::Function>()) {
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
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const lua_glue::Object rawReceiver = writeValue(lua, receiver);
    const lua_glue::Object callable =
        ludork::standard::class_runtime::protectedGet(
            lua, rawReceiver, lua_glue::MakeObject(lua, name));
    if (!callable.is<lua_glue::Function>()) {
        throw std::runtime_error("Runtime method is not defined: " + name);
    }
    std::vector<lua_glue::Object> rawArguments;
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
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const lua_glue::Object rawCallable = writeValue(lua, callable);
    if (!rawCallable.is<lua_glue::Function>()) {
        throw std::invalid_argument("Runtime callable must be a function");
    }
    return invokeAndRead(lua, rawCallable, writeArguments(lua, arguments),
                         "runtime callable arguments");
}

RuntimeValue RuntimeReflectionFacade::clone(const RuntimeValue& value) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    return readValue(
        ludork::standard::class_runtime::deepCopy(lua, writeValue(lua, value)));
}

RuntimeReflectionFacade& runtimeReflection() {
    static RuntimeReflectionFacade reflection;
    return reflection;
}
