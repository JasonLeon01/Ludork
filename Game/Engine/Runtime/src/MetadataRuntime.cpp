#include <Runtime/MetadataRuntime.hpp>

#include "LuaServices/RuntimeBindingTraits.hpp"
#include "LuaServices/RuntimeServiceInternals.hpp"
#include "Metadata/ConfigVarReferences.hpp"
#include <Runtime/RuntimeSession.hpp>

#include <ClassServices.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

lua_glue::Object writeValue(lua_glue::StateView lua,
                            const RuntimeValue& value) {
    return ludork::runtime::binding::writeLuaValue(lua, value);
}

RuntimeValue readValue(const lua_glue::Object& value) {
    return ludork::runtime::binding::readLuaValue<RuntimeValue>(value);
}
}  // namespace

RuntimeValue::Map MetadataRuntimeFacade::configVars(
    const RuntimeValue& metadata) const {
    return ludork::runtime::detail::parseConfigVarReferences(metadata);
}

std::optional<std::string> MetadataRuntimeFacade::classModulePath(
    const RuntimeValue& classReference) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const lua_glue::Object module =
        ludork::runtime::detail::findRuntimeClassModule(
            lua, writeValue(lua, classReference));
    return module.is<std::string>()
               ? std::optional<std::string>(module.as<std::string>())
               : std::nullopt;
}

std::pair<RuntimeValue, RuntimeValue> MetadataRuntimeFacade::classTypeMetadata(
    const RuntimeValue& classReference) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const lua_glue::Object rawClass = writeValue(lua, classReference);
    if (!rawClass.is<lua_glue::Table>()) {
        return {};
    }
    const lua_glue::Table descriptor =
        ludork::runtime::detail::runtimeClassTypeDescriptor(
            lua, rawClass.as<lua_glue::Table>());
    const lua_glue::Object hasMetadata =
        descriptor.raw_get<lua_glue::Object>("hasMetadata");
    const lua_glue::Object metadata =
        hasMetadata.is<bool>() && hasMetadata.as<bool>()
            ? descriptor.raw_get<lua_glue::Object>("metadata")
            : ludork::runtime::detail::nilObject(lua);
    const lua_glue::Object module =
        descriptor.raw_get<lua_glue::Object>("module");
    return {
        readValue(metadata),
        readValue(module.valid() ? module
                                 : ludork::runtime::detail::nilObject(lua))};
}

RuntimeValue MetadataRuntimeFacade::attrMetadata(
    const RuntimeValue& owner) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const lua_glue::Object rawOwner = writeValue(lua, owner);
    if (!rawOwner.is<lua_glue::Table>()) {
        return RuntimeValue(RuntimeValue::Map{});
    }
    return readValue(lua_glue::MakeObject(
        lua, ludork::runtime::detail::collectRuntimeAttrMetadata(
                 lua, rawOwner.as<lua_glue::Table>())));
}

RuntimeValue MetadataRuntimeFacade::resolveAttrMetadata(
    const RuntimeValue& owner, const std::string& key) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const lua_glue::Object rawOwner = writeValue(lua, owner);
    if (!rawOwner.is<lua_glue::Table>()) {
        return {};
    }
    return readValue(ludork::runtime::detail::collectRuntimeAttrMetadata(
                         lua, rawOwner.as<lua_glue::Table>())
                         .raw_get<lua_glue::Object>(key));
}

RuntimeValue MetadataRuntimeFacade::resolveAttrValueType(
    const RuntimeValue& owner, const std::string& key) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua(runtime.state());
    return readValue(ludork::runtime::detail::resolveRuntimeAttrValueType(
        lua, writeValue(lua, owner), key));
}

std::pair<RuntimeValue, RuntimeValue> MetadataRuntimeFacade::resolveConfigVar(
    const RuntimeValue& owner, const std::string& key) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const auto [configName, settingName] =
        ludork::runtime::detail::resolveRuntimeConfigVar(
            lua, writeValue(lua, owner), lua_glue::MakeObject(lua, key));
    return {readValue(configName), readValue(settingName)};
}

std::pair<RuntimeValue, RuntimeValue>
MetadataRuntimeFacade::resolveMemberMetadata(const RuntimeValue& owner,
                                             const std::string& key) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const auto [metadata, declaringModule] =
        ludork::runtime::detail::resolveRuntimeMemberMetadata(
            lua, writeValue(lua, owner), lua_glue::MakeObject(lua, key));
    return {readValue(metadata), readValue(declaringModule)};
}

RuntimeValue MetadataRuntimeFacade::evaluateExpression(
    const RuntimeValue& value, const RuntimeValue::Map& environment) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    return readValue(ludork::runtime::detail::evaluateRuntimeExpression(
        lua, writeValue(lua, value),
        writeValue(lua, RuntimeValue(environment))));
}

RuntimeValue MetadataRuntimeFacade::resolveType(
    const RuntimeValue& typeReference,
    const std::string& declaringModule) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    return readValue(ludork::runtime::detail::resolveRuntimeMetadataType(
        lua, writeValue(lua, typeReference),
        lua_glue::MakeObject(lua, declaringModule)));
}

RuntimeValue MetadataRuntimeFacade::constructTypedValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const std::string& declaringModule) const {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua = lua_glue::StateView(runtime.state());
    const lua_glue::Object rawValue = writeValue(lua, value);
    const lua_glue::Object target =
        ludork::runtime::detail::resolveRuntimeMetadataType(
            lua, writeValue(lua, valueType),
            lua_glue::MakeObject(lua, declaringModule));
    if (!target.valid() || target.get_type() == lua_glue::Type::Nil) {
        throw std::runtime_error("Cannot resolve runtime metadata type");
    }
    if (target.get_type() == lua_glue::Type::Table &&
        ludork::standard::class_runtime::isInstanceOf(
            lua, rawValue, target.as<lua_glue::Table>())) {
        return value;
    }
    if (rawValue.get_type() != lua_glue::Type::Table) {
        return value;
    }
    std::vector<lua_glue::Object> constructorArguments;
    if (!ludork::runtime::detail::runtimeSequence(
            rawValue.as<lua_glue::Table>(), constructorArguments)) {
        constructorArguments = {rawValue};
    }
    if (constructorArguments.size() == 1 &&
        constructorArguments.front().get_type() == lua_glue::Type::Table) {
        std::vector<lua_glue::Object> nested;
        if (ludork::runtime::detail::runtimeSequence(
                constructorArguments.front().as<lua_glue::Table>(), nested)) {
            constructorArguments = std::move(nested);
        }
    }
    if (target.get_type() != lua_glue::Type::Table) {
        throw std::runtime_error("Runtime metadata type is not constructible");
    }
    const lua_glue::Object constructor =
        ludork::standard::class_runtime::protectedGet(
            lua, target, lua_glue::MakeObject(lua, "new"));
    if (!constructor.is<lua_glue::Function>()) {
        throw std::runtime_error(
            "Runtime metadata type has no new constructor");
    }

    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    try {
        const int resultCount = ludork::runtime::detail::invokeRuntimeFunction(
            state, constructor, constructorArguments,
            "runtime constructor arguments");
        RuntimeValue result = resultCount == 0
                                  ? RuntimeValue()
                                  : readValue(lua_glue::Read<lua_glue::Object>(
                                        state, stackBase + 1));
        lua_settop(state, stackBase);
        return result;
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

MetadataRuntimeFacade& metadataRuntime() {
    static MetadataRuntimeFacade runtime;
    return runtime;
}
