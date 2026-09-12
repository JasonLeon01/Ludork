#include <Runtime/MetadataRuntime.hpp>

#include "LuaServices/RuntimeBindingTraits.hpp"
#include "LuaServices/RuntimeServiceInternals.hpp"
#include "Metadata/ConfigVarReferences.hpp"
#include <Runtime/RuntimeSession.hpp>

#include <ClassServices.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>

#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

sol::object writeValue(sol::state_view lua, const RuntimeValue& value) {
    return ludork::runtime::binding::writeLuaValue(lua, value);
}

RuntimeValue readValue(const sol::object& value) {
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
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object module = ludork::runtime::detail::findRuntimeClassModule(
        lua, writeValue(lua, classReference));
    return module.is<std::string>()
               ? std::optional<std::string>(module.as<std::string>())
               : std::nullopt;
}

std::pair<RuntimeValue, RuntimeValue> MetadataRuntimeFacade::classTypeMetadata(
    const RuntimeValue& classReference) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object rawClass = writeValue(lua, classReference);
    if (!rawClass.is<sol::table>()) {
        return {};
    }
    sol::table cache = ludork::runtime::detail::registryTable(
        lua, ludork::runtime::detail::CLASS_TYPE_METADATA_CACHE_KEY, "k");
    sol::object rawDescriptor = cache.raw_get<sol::object>(rawClass);
    if (!rawDescriptor.is<sol::table>()) {
        sol::object metadata =
            ludork::runtime::detail::syntheticRuntimeMetadata(
                lua, rawClass.as<sol::table>());
        if (!metadata.is<sol::table>()) {
            metadata = ludork::runtime::detail::runtimeTypeMetadata(
                lua, rawClass.as<sol::table>());
        }
        sol::table descriptor = lua.create_table();
        descriptor.raw_set("hasMetadata", metadata.is<sol::table>());
        if (metadata.is<sol::table>()) {
            descriptor.raw_set("metadata", metadata);
        }
        const sol::object module =
            ludork::runtime::detail::findRuntimeClassModule(lua, rawClass);
        if (module.valid() && module.get_type() != sol::type::lua_nil) {
            descriptor.raw_set("module", module);
        }
        cache.raw_set(rawClass, descriptor);
        rawDescriptor = sol::make_object(lua, descriptor);
    }
    const sol::table descriptor = rawDescriptor.as<sol::table>();
    const sol::object hasMetadata =
        descriptor.raw_get<sol::object>("hasMetadata");
    const sol::object metadata =
        hasMetadata.is<bool>() && hasMetadata.as<bool>()
            ? descriptor.raw_get<sol::object>("metadata")
            : ludork::runtime::detail::nilObject(lua);
    const sol::object module = descriptor.raw_get<sol::object>("module");
    return {
        readValue(metadata),
        readValue(module.valid() ? module
                                 : ludork::runtime::detail::nilObject(lua))};
}

RuntimeValue MetadataRuntimeFacade::attrMetadata(
    const RuntimeValue& owner) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object rawOwner = writeValue(lua, owner);
    if (!rawOwner.is<sol::table>()) {
        return RuntimeValue(RuntimeValue::Map{});
    }
    return readValue(sol::make_object(
        lua, ludork::runtime::detail::collectRuntimeAttrMetadata(
                 lua, rawOwner.as<sol::table>())));
}

RuntimeValue MetadataRuntimeFacade::resolveAttrMetadata(
    const RuntimeValue& owner, const std::string& key) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object rawOwner = writeValue(lua, owner);
    if (!rawOwner.is<sol::table>()) {
        return {};
    }
    return readValue(ludork::runtime::detail::collectRuntimeAttrMetadata(
                         lua, rawOwner.as<sol::table>())
                         .raw_get<sol::object>(key));
}

RuntimeValue MetadataRuntimeFacade::resolveAttrValueType(
    const RuntimeValue& owner, const std::string& key) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua(runtime.state());
    return readValue(ludork::runtime::detail::resolveRuntimeAttrValueType(
        lua, writeValue(lua, owner), key));
}

std::pair<RuntimeValue, RuntimeValue> MetadataRuntimeFacade::resolveConfigVar(
    const RuntimeValue& owner, const std::string& key) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const auto [configName, settingName] =
        ludork::runtime::detail::resolveRuntimeConfigVar(
            lua, writeValue(lua, owner), sol::make_object(lua, key));
    return {readValue(configName), readValue(settingName)};
}

std::pair<RuntimeValue, RuntimeValue>
MetadataRuntimeFacade::resolveMemberMetadata(const RuntimeValue& owner,
                                             const std::string& key) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const auto [metadata, declaringModule] =
        ludork::runtime::detail::resolveRuntimeMemberMetadata(
            lua, writeValue(lua, owner), sol::make_object(lua, key));
    return {readValue(metadata), readValue(declaringModule)};
}

RuntimeValue MetadataRuntimeFacade::evaluateExpression(
    const RuntimeValue& value, const RuntimeValue::Map& environment) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    return readValue(ludork::runtime::detail::evaluateRuntimeExpression(
        lua, writeValue(lua, value),
        writeValue(lua, RuntimeValue(environment))));
}

RuntimeValue MetadataRuntimeFacade::resolveType(
    const RuntimeValue& typeReference,
    const std::string& declaringModule) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    return readValue(ludork::runtime::detail::resolveRuntimeMetadataType(
        lua, writeValue(lua, typeReference),
        sol::make_object(lua, declaringModule)));
}

RuntimeValue MetadataRuntimeFacade::constructTypedValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const std::string& declaringModule) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object rawValue = writeValue(lua, value);
    const sol::object target =
        ludork::runtime::detail::resolveRuntimeMetadataType(
            lua, writeValue(lua, valueType),
            sol::make_object(lua, declaringModule));
    if (!target.valid() || target.get_type() == sol::type::lua_nil) {
        throw std::runtime_error("Cannot resolve runtime metadata type");
    }
    if (target.get_type() == sol::type::table &&
        ludork::standard::class_runtime::isInstanceOf(
            lua, rawValue, target.as<sol::table>())) {
        return value;
    }
    if (rawValue.get_type() != sol::type::table) {
        return value;
    }
    std::vector<sol::object> constructorArguments;
    if (!ludork::runtime::detail::runtimeSequence(rawValue.as<sol::table>(),
                                                  constructorArguments)) {
        constructorArguments = {rawValue};
    }
    if (constructorArguments.size() == 1 &&
        constructorArguments.front().get_type() == sol::type::table) {
        std::vector<sol::object> nested;
        if (ludork::runtime::detail::runtimeSequence(
                constructorArguments.front().as<sol::table>(), nested)) {
            constructorArguments = std::move(nested);
        }
    }
    if (target.get_type() != sol::type::table) {
        throw std::runtime_error("Runtime metadata type is not constructible");
    }
    const sol::object constructor =
        ludork::standard::class_runtime::protectedGet(
            lua, target, sol::make_object(lua, "new"));
    if (!constructor.is<sol::protected_function>()) {
        throw std::runtime_error(
            "Runtime metadata type has no new constructor");
    }

    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    try {
        const int resultCount = ludork::runtime::detail::invokeRuntimeFunction(
            state, constructor, constructorArguments,
            "runtime constructor arguments");
        RuntimeValue result =
            resultCount == 0
                ? RuntimeValue()
                : readValue(sol::stack::get<sol::object>(state, stackBase + 1));
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
