#include <Runtime/MetadataRuntime.hpp>

#include "RuntimeBindingTraits.hpp"
#include <Runtime/RuntimeSession.hpp>
#include "RuntimeServiceInternals.hpp"

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

constexpr const char* CLASS_TYPE_METADATA_CACHE_KEY =
    "Ludork.Runtime.classTypeMetadataCache";

sol::object writeValue(sol::state_view lua, const RuntimeValue& value) {
    return ludork::runtime::binding::writeLuaValue(lua, value);
}

RuntimeValue readValue(const sol::object& value) {
    return ludork::runtime::binding::readLuaValue<RuntimeValue>(value);
}

std::optional<RuntimeValueView> mapValue(RuntimeValueView value,
                                         const std::string& key) {
    const auto map = value.map();
    return map ? map->find(key) : std::nullopt;
}

void addConfigVarReference(RuntimeValue::Map& result, const std::string& name,
                           RuntimeValueView reference) {
    if (name.empty()) {
        return;
    }
    if (const std::string* text = reference.getIf<std::string>()) {
        if (text->empty()) {
            return;
        }
        const std::size_t separator = text->find('.');
        if (separator == std::string::npos) {
            result[name] = RuntimeValue(
                RuntimeValue::Array{RuntimeValue(*text), RuntimeValue(name)});
            return;
        }
        const std::string configName = text->substr(0, separator);
        const std::string settingName = text->substr(separator + 1);
        if (!configName.empty() && !settingName.empty()) {
            result[name] = RuntimeValue(RuntimeValue::Array{
                RuntimeValue(configName), RuntimeValue(settingName)});
        }
        return;
    }
    std::optional<RuntimeArrayView> values =
        RuntimeValueView(reference).array();
    if (!values || values->size() < 2) {
        return;
    }
    const std::string* configName = (*values)[0].getIf<std::string>();
    const std::string* settingName = (*values)[1].getIf<std::string>();
    if (configName == nullptr || configName->empty() ||
        settingName == nullptr || settingName->empty()) {
        return;
    }
    result[name] = RuntimeValue(RuntimeValue::Array{
        RuntimeValue(*configName), RuntimeValue(*settingName)});
}

void addConfigVarItem(RuntimeValue::Map& result, RuntimeValueView item) {
    if (const std::string* name = item.getIf<std::string>()) {
        if (!name->empty()) {
            result[*name] = RuntimeValue(RuntimeValue::Array{
                RuntimeValue(std::string("System")), RuntimeValue(*name)});
        }
        return;
    }
    std::optional<RuntimeArrayView> values = RuntimeValueView(item).array();
    if (!values || values->size() < 2) {
        return;
    }
    const std::string* name = (*values)[0].getIf<std::string>();
    if (name == nullptr || name->empty()) {
        return;
    }
    if (values->size() >= 3) {
        const RuntimeValue reference(RuntimeValue::Array{
            (*values)[1].toValue(), (*values)[2].toValue()});
        addConfigVarReference(result, *name, reference);
        return;
    }
    addConfigVarReference(result, *name, (*values)[1]);
}

}  // namespace

RuntimeValue::Map MetadataRuntimeFacade::configVars(
    const RuntimeValue& metadata) const {
    RuntimeValue::Map result;
    const auto rawVars = mapValue(metadata, "ConfigVars");
    if (!rawVars) {
        return result;
    }
    if (std::optional<RuntimeMapView> values =
            RuntimeValueView(*rawVars).map()) {
        for (const auto& [name, reference] : *values) {
            addConfigVarReference(result, name, reference);
        }
        return result;
    }
    if (std::optional<RuntimeArrayView> values =
            RuntimeValueView(*rawVars).array()) {
        for (RuntimeValueView item : *values) {
            addConfigVarItem(result, item);
        }
    }
    return result;
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
        lua, CLASS_TYPE_METADATA_CACHE_KEY, "k");
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
    if (target.is<sol::table>() &&
        ludork::standard::class_runtime::isInstanceOf(
            lua, rawValue, target.as<sol::table>())) {
        return value;
    }
    if (!rawValue.is<sol::table>()) {
        return value;
    }
    std::vector<sol::object> constructorArguments;
    if (!ludork::runtime::detail::runtimeSequence(rawValue.as<sol::table>(),
                                                  constructorArguments)) {
        return value;
    }
    if (constructorArguments.size() == 1 &&
        constructorArguments.front().is<sol::table>()) {
        std::vector<sol::object> nested;
        if (ludork::runtime::detail::runtimeSequence(
                constructorArguments.front().as<sol::table>(), nested)) {
            constructorArguments = std::move(nested);
        }
    }
    if (!target.is<sol::table>()) {
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
