#include "RuntimeSubsystemServices.hpp"

#include <ClassServices.hpp>

#include <stdexcept>
#include <utility>

namespace ludork::engine::runtime_detail {
namespace {

constexpr const char* CLASS_TYPE_METADATA_CACHE_KEY =
    "Ludork.Engine.classTypeMetadataCache";

}  // namespace

const std::vector<std::string>& metadataRuntimeServiceNames() {
    static const std::vector<std::string> names{
        "resolveConfigVar",     "resolveMemberMetadata", "getClassModulePath",
        "getClassTypeMetadata", "getAttrMetadata",       "resolveAttrMetadata",
        "resolveAttrValueType", "evalDataExpression",    "resolveMetadataType",
        "constructTypedValue",
    };
    return names;
}

ServiceDispatchResult dispatchMetadataRuntimeService(
    sol::this_state state, const std::string& operation,
    const sol::table& arguments) {
    sol::state_view lua(state);
    const sol::object first = runtimeResolverArgument(lua, arguments, 1);
    const sol::object second = runtimeResolverArgument(lua, arguments, 2);
    if (operation == "resolveConfigVar") {
        const auto [configName, settingName] =
            resolveRuntimeConfigVar(lua, first, second);
        return runtimeResolverResult(lua, {configName, settingName});
    }
    if (operation == "resolveMemberMetadata") {
        const auto [metadata, declaringModule] =
            resolveRuntimeMemberMetadata(lua, first, second);
        return runtimeResolverResult(lua, {metadata, declaringModule});
    }
    if (operation == "getClassModulePath") {
        return runtimeResolverResult(lua, {findRuntimeClassModule(lua, first)});
    }
    if (operation == "getClassTypeMetadata") {
        if (!first.is<sol::table>()) {
            return runtimeResolverResult(lua, {nilObject(lua), nilObject(lua)});
        }
        sol::table cache =
            registryTable(lua, CLASS_TYPE_METADATA_CACHE_KEY, "k");
        sol::object rawDescriptor = cache.raw_get<sol::object>(first);
        if (!rawDescriptor.is<sol::table>()) {
            sol::object metadata =
                syntheticRuntimeMetadata(lua, first.as<sol::table>());
            if (metadata.get_type() == sol::type::lua_nil) {
                metadata = runtimeTypeMetadata(lua, first.as<sol::table>());
            }
            sol::table descriptor = lua.create_table();
            descriptor.raw_set(
                "hasMetadata",
                metadata.valid() && metadata.get_type() != sol::type::lua_nil);
            if (metadata.valid() && metadata.get_type() != sol::type::lua_nil) {
                descriptor.raw_set("metadata", metadata);
            }
            const sol::object module = findRuntimeClassModule(lua, first);
            if (module.valid() && module.get_type() != sol::type::lua_nil) {
                descriptor.raw_set("module", module);
            }
            cache.raw_set(first, descriptor);
            rawDescriptor = sol::make_object(lua, descriptor);
        }
        const sol::table descriptor = rawDescriptor.as<sol::table>();
        const sol::object rawHasMetadata =
            descriptor.raw_get<sol::object>("hasMetadata");
        const sol::object metadata =
            rawHasMetadata.is<bool>() && rawHasMetadata.as<bool>()
                ? descriptor.raw_get<sol::object>("metadata")
                : nilObject(lua);
        const sol::object module = descriptor.raw_get<sol::object>("module");
        return runtimeResolverResult(
            lua, {metadata, module.valid() ? module : nilObject(lua)});
    }
    if (operation == "getAttrMetadata") {
        const sol::object metadata =
            first.is<sol::table>()
                ? sol::make_object(lua, collectRuntimeAttrMetadata(
                                            lua, first.as<sol::table>()))
                : sol::make_object(lua, lua.create_table());
        return runtimeResolverResult(lua, {metadata});
    }
    if (operation == "resolveAttrMetadata") {
        sol::object metadata = nilObject(lua);
        if (first.is<sol::table>() && second.is<std::string>()) {
            metadata = collectRuntimeAttrMetadata(lua, first.as<sol::table>())
                           .raw_get<sol::object>(second.as<std::string>());
        }
        return runtimeResolverResult(lua, {metadata});
    }
    if (operation == "resolveAttrValueType") {
        sol::object valueType = nilObject(lua);
        if (first.is<sol::table>() && second.is<std::string>()) {
            const sol::table metadata =
                collectRuntimeAttrMetadata(lua, first.as<sol::table>());
            const sol::object descriptor =
                metadata.raw_get<sol::object>(second.as<std::string>());
            if (descriptor.is<sol::table>()) {
                valueType =
                    descriptor.as<sol::table>().raw_get<sol::object>("type");
            }
            if (valueType.get_type() == sol::type::lua_nil) {
                const sol::object value =
                    first.as<sol::table>().get<sol::object>(
                        second.as<std::string>());
                switch (value.get_type()) {
                    case sol::type::boolean:
                        valueType = sol::make_object(lua, "bool");
                        break;
                    case sol::type::number: {
                        value.push();
                        const bool integer =
                            lua_isinteger(lua.lua_state(), -1) != 0;
                        lua_pop(lua.lua_state(), 1);
                        valueType =
                            sol::make_object(lua, integer ? "int" : "float");
                        break;
                    }
                    case sol::type::string:
                        valueType = sol::make_object(lua, "string");
                        break;
                    case sol::type::table:
                        valueType = sol::make_object(lua, "table");
                        break;
                    case sol::type::userdata: {
                        const sol::table metatable =
                            objectMetatable(lua, value);
                        const sol::object declared =
                            metatable.raw_get<sol::object>("__metadataType");
                        if (declared.is<std::string>()) {
                            valueType = declared;
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        if (valueType.get_type() == sol::type::lua_nil) {
            valueType = sol::make_object(lua, "any");
        }
        return runtimeResolverResult(lua, {valueType});
    }
    if (operation == "evalDataExpression") {
        return runtimeResolverResult(
            lua, {evaluateRuntimeExpression(lua, first, second)});
    }
    if (operation == "resolveMetadataType") {
        return runtimeResolverResult(
            lua, {resolveRuntimeMetadataType(lua, first, second)});
    }
    if (operation == "constructTypedValue") {
        const sol::object declaringModule =
            runtimeResolverArgument(lua, arguments, 3);
        const sol::object target =
            resolveRuntimeMetadataType(lua, second, declaringModule);
        if (target.get_type() == sol::type::lua_nil) {
            throw std::runtime_error("Cannot resolve runtime metadata type");
        }
        if (target.is<sol::table>() &&
            ludork::standard::class_runtime::isInstanceOf(
                lua, first, target.as<sol::table>())) {
            return runtimeResolverResult(lua, {first});
        }
        if (!first.is<sol::table>()) {
            return runtimeResolverResult(lua, {first});
        }
        std::vector<sol::object> constructorArguments;
        if (!runtimeSequence(first.as<sol::table>(), constructorArguments)) {
            return runtimeResolverResult(lua, {first});
        }
        if (constructorArguments.size() == 1 &&
            constructorArguments.front().is<sol::table>()) {
            std::vector<sol::object> nested;
            if (runtimeSequence(constructorArguments.front().as<sol::table>(),
                                nested)) {
                constructorArguments = std::move(nested);
            }
        }
        if (!target.is<sol::table>()) {
            throw std::runtime_error(
                "Runtime metadata type is not constructible");
        }
        const sol::object rawConstructor =
            ludork::standard::class_runtime::protectedGet(
                lua, target, sol::make_object(lua, "new"));
        if (!rawConstructor.is<sol::protected_function>()) {
            throw std::runtime_error(
                "Runtime metadata type has no new constructor");
        }
        sol::table packedArguments =
            lua.create_table(static_cast<int>(constructorArguments.size()), 1);
        packedArguments.raw_set("n", constructorArguments.size());
        for (std::size_t index = 0; index < constructorArguments.size();
             ++index) {
            packedArguments.raw_set(index + 1, constructorArguments[index]);
        }
        const sol::table constructed = ludork::standard::class_runtime::invoke(
            lua, rawConstructor, packedArguments);
        return runtimeResolverResult(
            lua, {runtimeResolverArgument(lua, constructed, 1)});
    }
    return std::nullopt;
}

}  // namespace ludork::engine::runtime_detail
