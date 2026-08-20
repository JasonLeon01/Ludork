#include <Runtime/EngineRuntimeServices.hpp>

#include "RuntimeServiceInternals.hpp"
#include "RuntimeSubsystemServices.hpp"

#include <ClassServices.hpp>
#include <Gameplay/Components/ComponentRuntime.hpp>
#include <LuaError.hpp>
#include <LudorkCoreBinding/DynamicValueCodec.hpp>
#include <NodeGraph/Graph.hpp>
#include <Runtime/EngineClassRuntime.hpp>
#include <Runtime/NodeGraphRuntime.hpp>
#include <RuntimeSession.hpp>
#include <Utf8Path.hpp>
#include <Utils/DataValue.hpp>
#include <Utils/EventBus.hpp>
#include <Utils/Inner.hpp>

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <climits>
#include <cstddef>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::engine::runtime_detail {

constexpr const char* CLASS_IDENTITY_CACHE_KEY =
    "Ludork.Engine.classIdentityCache";
constexpr const char* CLASS_TYPE_METADATA_CACHE_KEY =
    "Ludork.Engine.classTypeMetadataCache";
constexpr const char* ATTR_METADATA_CACHE_KEY =
    "Ludork.Engine.attrMetadataCache";

struct RuntimeClassIdentity {
    sol::table descriptor;
    std::string module;
    std::string type;
    bool direct = false;
};

RuntimeClassIdentity classIdentityFromDescriptor(const sol::table& descriptor) {
    const sol::object rawModule = descriptor.raw_get<sol::object>("module");
    const sol::object rawType = descriptor.raw_get<sol::object>("type");
    return {
        descriptor,
        rawModule.is<std::string>() ? rawModule.as<std::string>()
                                    : std::string(),
        rawType.is<std::string>() ? rawType.as<std::string>() : std::string(),
        rawBool(descriptor, "direct")};
}

std::optional<RuntimeClassIdentity> resolveRuntimeClassIdentity(
    sol::state_view lua, const sol::object& classReference) {
    if (!classReference.is<sol::table>()) {
        return std::nullopt;
    }
    const sol::table classTable = classReference.as<sol::table>();
    sol::table cache = registryTable(lua, CLASS_IDENTITY_CACHE_KEY, "k");
    const sol::object cached = cache.raw_get<sol::object>(classReference);
    if (cached.is<sol::table>()) {
        return classIdentityFromDescriptor(cached.as<sol::table>());
    }

    const sol::object explicitModule =
        classTable.raw_get<sol::object>("__metadataModule");
    if (explicitModule.is<std::string>() &&
        !explicitModule.as<std::string>().empty()) {
        sol::table descriptor = lua.create_table();
        descriptor.raw_set("module", explicitModule);
        descriptor.raw_set("direct", false);
        cache.raw_set(classReference, descriptor);
        return classIdentityFromDescriptor(descriptor);
    }

    const sol::object rawPackage =
        lua.globals().raw_get<sol::object>("package");
    if (!rawPackage.is<sol::table>()) {
        return std::nullopt;
    }
    const sol::object rawLoaded =
        rawPackage.as<sol::table>().raw_get<sol::object>("loaded");
    if (!rawLoaded.is<sol::table>()) {
        return std::nullopt;
    }
    const sol::table loaded = rawLoaded.as<sol::table>();

    std::vector<std::string> directModules;
    for (const auto& entry : loaded) {
        if (entry.first.is<std::string>() &&
            rawEqual(lua, entry.second, classReference)) {
            directModules.push_back(entry.first.as<std::string>());
        }
    }
    std::sort(directModules.begin(), directModules.end());
    directModules.erase(std::unique(directModules.begin(), directModules.end()),
                        directModules.end());
    if (directModules.size() > 1) {
        throw std::runtime_error("Lua class is returned by multiple modules: " +
                                 directModules.front() + ", " +
                                 directModules.back());
    }
    if (!directModules.empty()) {
        sol::table descriptor = lua.create_table();
        descriptor.raw_set("module", directModules.front());
        descriptor.raw_set("direct", true);
        cache.raw_set(classReference, descriptor);
        return classIdentityFromDescriptor(descriptor);
    }

    std::vector<std::pair<std::string, std::string>> exports;
    for (const auto& entry : loaded) {
        if (!entry.first.is<std::string>() || !entry.second.is<sol::table>()) {
            continue;
        }
        const std::string moduleName = entry.first.as<std::string>();
        for (const auto& member : entry.second.as<sol::table>()) {
            if (!member.first.is<std::string>()) {
                continue;
            }
            const std::string exportName = member.first.as<std::string>();
            if (exportName.rfind("__", 0) == 0 ||
                exportName == "_hasImplementationOwner") {
                continue;
            }
            if (rawEqual(lua, member.second, classReference)) {
                exports.emplace_back(moduleName, exportName);
            }
        }
    }
    std::sort(exports.begin(), exports.end());
    exports.erase(std::unique(exports.begin(), exports.end()), exports.end());
    if (exports.size() > 1) {
        throw std::runtime_error(
            "Lua class has multiple module exports: " + exports.front().first +
            "." + exports.front().second + ", " + exports.back().first + "." +
            exports.back().second);
    }
    if (exports.empty()) {
        return std::nullopt;
    }

    sol::table descriptor = lua.create_table();
    descriptor.raw_set("module", exports.front().first);
    descriptor.raw_set("type", exports.front().second);
    descriptor.raw_set("direct", false);
    cache.raw_set(classReference, descriptor);
    return classIdentityFromDescriptor(descriptor);
}

sol::object findRuntimeClassModule(sol::state_view lua,
                                   const sol::object& classReference) {
    const std::optional<RuntimeClassIdentity> identity =
        resolveRuntimeClassIdentity(lua, classReference);
    return identity.has_value() && !identity->module.empty()
               ? sol::make_object(lua, identity->module)
               : nilObject(lua);
}

sol::object syntheticRuntimeMetadata(sol::state_view lua,
                                     const sol::table& classTable) {
    const sol::object native =
        classTable.raw_get<sol::object>("__runtimeMetadata");
    if (native.is<sol::table>()) {
        return native;
    }
    const sol::object rawTypes = classTable.raw_get<sol::object>("__types");
    const sol::object rawMeta =
        classTable.raw_get<sol::object>("__runtimeMeta");
    if (!rawTypes.is<sol::table>() && !rawMeta.is<sol::table>()) {
        return nilObject(lua);
    }

    sol::table metadata = lua.create_table();
    sol::table attrs = lua.create_table();
    std::size_t index = 1;
    if (rawTypes.is<sol::table>()) {
        for (const auto& entry : rawTypes.as<sol::table>()) {
            if (!entry.first.is<std::string>()) {
                continue;
            }
            const std::string name = entry.first.as<std::string>();
            attrs.raw_set(index++, name);
            sol::table member = lua.create_table();
            member.raw_set("type", entry.second);
            metadata.raw_set(name, member);
        }
    }
    metadata.raw_set("attrs", attrs);
    if (rawMeta.is<sol::table>()) {
        metadata.raw_set("Meta", rawMeta);
    }
    return sol::make_object(lua, metadata);
}

sol::object runtimeTypeMetadata(sol::state_view lua,
                                const sol::table& classType);

sol::table collectRuntimeAttrMetadata(sol::state_view lua,
                                      const sol::table& owner) {
    sol::table cache = registryTable(lua, ATTR_METADATA_CACHE_KEY, "k");
    const sol::object ownerObject = sol::make_object(lua, owner);
    const sol::object cached = cache.raw_get<sol::object>(ownerObject);
    if (cached.is<sol::table>()) {
        return cached.as<sol::table>();
    }
    sol::table result = lua.create_table();
    const std::vector<sol::table> mro = runtimeClassMro(lua, owner);
    for (auto current = mro.rbegin(); current != mro.rend(); ++current) {
        sol::object rawMetadata = syntheticRuntimeMetadata(lua, *current);
        if (!rawMetadata.is<sol::table>()) {
            rawMetadata = runtimeTypeMetadata(lua, *current);
        }
        if (!rawMetadata.is<sol::table>()) {
            continue;
        }
        const sol::table metadata = rawMetadata.as<sol::table>();
        const sol::object rawAttrs = metadata.raw_get<sol::object>("attrs");
        if (!rawAttrs.is<sol::table>()) {
            continue;
        }
        const sol::object module =
            findRuntimeClassModule(lua, sol::make_object(lua, *current));
        const sol::table attrs = rawAttrs.as<sol::table>();
        for (std::size_t index = 1; index <= attrs.size(); ++index) {
            const sol::object rawName = attrs.raw_get<sol::object>(index);
            if (!rawName.is<std::string>()) {
                continue;
            }
            const std::string name = rawName.as<std::string>();
            const sol::object rawMember = metadata.raw_get<sol::object>(name);
            if (!rawMember.is<sol::table>()) {
                continue;
            }
            const sol::table member = rawMember.as<sol::table>();
            const sol::object type = member.raw_get<sol::object>("type");
            if (type.get_type() == sol::type::lua_nil) {
                continue;
            }
            sol::table descriptor = lua.create_table();
            descriptor.raw_set("type", type);
            const sol::object component =
                member.raw_get<sol::object>("component");
            descriptor.raw_set("component",
                               component.is<bool>() && component.as<bool>());
            const sol::object declaredModule =
                member.raw_get<sol::object>("module");
            if (declaredModule.is<std::string>()) {
                descriptor.raw_set("module", declaredModule);
            } else if (module.is<std::string>()) {
                descriptor.raw_set("module", module);
            }
            descriptor.raw_set("metadata", member);
            result.raw_set(name, descriptor);
        }
    }
    for (auto current = mro.rbegin(); current != mro.rend(); ++current) {
        const sol::object rawTypes = current->raw_get<sol::object>("__types");
        if (!rawTypes.is<sol::table>()) {
            continue;
        }
        const sol::object module =
            findRuntimeClassModule(lua, sol::make_object(lua, *current));
        for (const auto& entry : rawTypes.as<sol::table>()) {
            if (!entry.first.is<std::string>()) {
                continue;
            }
            const std::string name = entry.first.as<std::string>();
            const sol::object existing = result.raw_get<sol::object>(name);
            sol::table descriptor = lua.create_table();
            descriptor.raw_set("type", entry.second);
            if (existing.is<sol::table>()) {
                const sol::table previous = existing.as<sol::table>();
                const sol::object component =
                    previous.raw_get<sol::object>("component");
                descriptor.raw_set(
                    "component", component.is<bool>() && component.as<bool>());
                const sol::object previousModule =
                    previous.raw_get<sol::object>("module");
                if (previousModule.is<std::string>()) {
                    descriptor.raw_set("module", previousModule);
                }
                const sol::object metadata =
                    previous.raw_get<sol::object>("metadata");
                if (metadata.get_type() != sol::type::lua_nil) {
                    descriptor.raw_set("metadata", metadata);
                }
            } else {
                descriptor.raw_set("component", false);
                if (module.is<std::string>()) {
                    descriptor.raw_set("module", module);
                }
            }
            result.raw_set(name, descriptor);
        }
    }
    cache.raw_set(ownerObject, result);
    return result;
}

std::pair<sol::object, sol::object> resolveRuntimeConfigVar(
    sol::state_view lua, const sol::object& owner, const sol::object& rawName) {
    if (!rawName.is<std::string>()) {
        return {nilObject(lua), nilObject(lua)};
    }
    sol::object rawClass = owner;
    if (!rawClass.is<sol::table>()) {
        rawClass = ludork::standard::class_runtime::typeOf(lua, owner);
    }
    if (!rawClass.is<sol::table>()) {
        return {nilObject(lua), nilObject(lua)};
    }
    const std::string name = rawName.as<std::string>();
    for (const sol::table& current :
         runtimeClassMro(lua, rawClass.as<sol::table>())) {
        sol::object metadata = syntheticRuntimeMetadata(lua, current);
        if (!metadata.is<sol::table>()) {
            metadata = runtimeTypeMetadata(lua, current);
        }
        if (!metadata.is<sol::table>()) {
            continue;
        }
        const RuntimeValue metadataValue =
            ludork_core::readLuaValue<RuntimeValue>(metadata);
        const RuntimeValue::Map* metadataFields =
            metadataValue.getIf<RuntimeValue::Map>();
        if (metadataFields == nullptr) {
            continue;
        }
        const auto metaIterator = metadataFields->find("Meta");
        if (metaIterator == metadataFields->end()) {
            continue;
        }
        const RuntimeValue::Map references =
            getConfigVars(metaIterator->second);
        const auto iterator = references.find(name);
        if (iterator == references.end()) {
            continue;
        }
        const RuntimeValue::Array* reference =
            iterator->second.getIf<RuntimeValue::Array>();
        if (reference == nullptr || reference->size() < 2) {
            continue;
        }
        return {
            ludork_core::writeLuaValue(lua, (*reference)[0]),
            ludork_core::writeLuaValue(lua, (*reference)[1]),
        };
    }
    return {nilObject(lua), nilObject(lua)};
}

std::pair<sol::object, sol::object> resolveRuntimeMemberMetadata(
    sol::state_view lua, const sol::object& owner, const sol::object& rawName) {
    if (!rawName.is<std::string>()) {
        return {nilObject(lua), nilObject(lua)};
    }
    const std::string name = rawName.as<std::string>();
    const sol::table mro =
        ludork::standard::class_runtime::getMroCopy(lua, owner);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawClass = mro.raw_get<sol::object>(index);
        if (!rawClass.is<sol::table>()) {
            continue;
        }
        sol::object metadata =
            syntheticRuntimeMetadata(lua, rawClass.as<sol::table>());
        if (!metadata.is<sol::table>()) {
            metadata = runtimeTypeMetadata(lua, rawClass.as<sol::table>());
        }
        if (!metadata.is<sol::table>()) {
            continue;
        }
        const sol::object member =
            metadata.as<sol::table>().raw_get<sol::object>(name);
        if (!member.is<sol::table>()) {
            continue;
        }
        const sol::object module = findRuntimeClassModule(lua, rawClass);
        return {member, module.valid() ? module : nilObject(lua)};
    }
    return {nilObject(lua), nilObject(lua)};
}

sol::object resolveRuntimePath(sol::state_view lua, const sol::object& root,
                               const std::string& path) {
    sol::object current = root;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t end = path.find('.', start);
        const std::string name = path.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (!current.is<sol::table>()) {
            return nilObject(lua);
        }
        current = current.as<sol::table>().get<sol::object>(name);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return current.valid() ? current : nilObject(lua);
}

std::optional<std::string> directRuntimeMetadataTypeName(
    sol::state_view lua, const std::string& modulePath) {
    const std::string metadataModule = modulePath + "_meta";
    const sol::object rawPackage =
        lua.globals().raw_get<sol::object>("package");
    if (!rawPackage.is<sol::table>()) {
        return std::nullopt;
    }
    const sol::table package = rawPackage.as<sol::table>();
    const sol::object rawSearchPath =
        package.raw_get<sol::object>("searchpath");
    const sol::object rawPath = package.raw_get<sol::object>("path");
    if (!rawSearchPath.is<sol::protected_function>() ||
        !rawPath.is<std::string>()) {
        return std::nullopt;
    }
    sol::protected_function searchPath =
        rawSearchPath.as<sol::protected_function>();
    sol::protected_function_result searched =
        searchPath(metadataModule, rawPath);
    const sol::object found = checkedResult(lua, searched);
    if (!found.is<std::string>()) {
        return std::nullopt;
    }

    const sol::table metadata = requireLuaTable(lua, metadataModule.c_str());
    std::optional<std::string> result;
    for (const auto& entry : metadata) {
        if (!entry.first.is<std::string>() || !entry.second.is<sol::table>()) {
            continue;
        }
        if (result.has_value()) {
            throw std::runtime_error(
                "Metadata module for directly returned class must contain one "
                "type: " +
                metadataModule);
        }
        result = entry.first.as<std::string>();
    }
    if (!result.has_value()) {
        throw std::runtime_error(
            "Metadata module for directly returned class contains no type: " +
            metadataModule);
    }
    return result;
}

sol::object requireRuntimeType(sol::state_view lua,
                               const std::string& modulePath,
                               const std::string& typeName) {
    const sol::object rawRequire =
        lua.globals().raw_get<sol::object>("require");
    if (!rawRequire.is<sol::protected_function>()) {
        return nilObject(lua);
    }
    sol::protected_function require = rawRequire.as<sol::protected_function>();
    sol::protected_function_result loaded = require(modulePath);
    const sol::object module = checkedResult(lua, loaded);
    if (!module.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::table table = module.as<sol::table>();
    if (isClass(table)) {
        const std::size_t separator = modulePath.find_last_of('.');
        const std::string moduleType = separator == std::string::npos
                                           ? modulePath
                                           : modulePath.substr(separator + 1);
        if (typeName == moduleType) {
            return module;
        }
        const std::optional<std::string> metadataType =
            directRuntimeMetadataTypeName(lua, modulePath);
        return metadataType.has_value() && *metadataType == typeName
                   ? module
                   : nilObject(lua);
    }
    return resolveRuntimePath(lua, module, typeName);
}

sol::object resolveRuntimeMetadataType(sol::state_view lua,
                                       const sol::object& typeReference,
                                       const sol::object& declaringModule) {
    if (typeReference.is<sol::table>()) {
        const sol::table reference = typeReference.as<sol::table>();
        const sol::object module = reference.raw_get<sol::object>(1);
        const sol::object name = reference.raw_get<sol::object>(2);
        if (!module.is<std::string>() || !name.is<std::string>()) {
            return nilObject(lua);
        }
        return requireRuntimeType(lua, module.as<std::string>(),
                                  name.as<std::string>());
    }
    if (!typeReference.is<std::string>()) {
        return nilObject(lua);
    }
    const std::string name = typeReference.as<std::string>();
    const sol::object global =
        resolveRuntimePath(lua, sol::make_object(lua, lua.globals()), name);
    if (global.get_type() != sol::type::lua_nil) {
        return global;
    }
    if (declaringModule.is<std::string>() &&
        name.find('.') == std::string::npos) {
        return requireRuntimeType(lua, declaringModule.as<std::string>(), name);
    }
    return nilObject(lua);
}

bool runtimeSequence(const sol::table& table,
                     std::vector<sol::object>& values) {
    const sol::object rawLength = table.raw_get<sol::object>("n");
    const bool packed = rawLength.is<lua_Integer>();
    if (packed && rawLength.as<lua_Integer>() < 0) {
        return false;
    }
    const std::size_t length =
        packed ? static_cast<std::size_t>(rawLength.as<lua_Integer>())
               : table.size();
    std::size_t itemCount = 0;
    for (const auto& entry : table) {
        if (packed && entry.first.is<std::string>() &&
            entry.first.as<std::string>() == "n") {
            continue;
        }
        if (!entry.first.is<std::int64_t>()) {
            return false;
        }
        const std::int64_t index = entry.first.as<std::int64_t>();
        if (index <= 0 || static_cast<std::size_t>(index) > length) {
            return false;
        }
        ++itemCount;
    }
    if (!packed && itemCount != length) {
        return false;
    }
    values.reserve(length);
    for (std::size_t index = 1; index <= length; ++index) {
        values.push_back(table.raw_get<sol::object>(index));
    }
    return true;
}

sol::object evaluateRuntimeExpression(sol::state_view lua,
                                      const sol::object& value,
                                      const sol::object& rawEnvironment) {
    if (!value.is<std::string>()) {
        return value;
    }
    const std::string expression = value.as<std::string>();
    sol::load_result loaded =
        lua.load("return " + expression, "=(data)", sol::load_mode::text);
    if (!loaded.valid()) {
        return value;
    }
    sol::protected_function function = loaded;
    sol::environment environment(lua, sol::create, lua.globals());
    if (rawEnvironment.is<sol::table>()) {
        for (const auto& entry : rawEnvironment.as<sol::table>()) {
            environment.raw_set(entry.first, entry.second);
        }
    }
    sol::set_environment(environment, function);
    sol::protected_function_result result = function();
    if (!result.valid()) {
        return value;
    }
    sol::object evaluated =
        result.return_count() == 0 ? nilObject(lua) : result.get<sol::object>();
    static const std::regex bareIdentifier(R"(^\s*[A-Za-z_][A-Za-z0-9_]*\s*$)");
    static const std::regex nilLiteral(R"(^\s*nil\s*$)");
    if (evaluated.get_type() == sol::type::lua_nil &&
        std::regex_match(expression, bareIdentifier) &&
        !std::regex_match(expression, nilLiteral)) {
        return value;
    }
    return evaluated;
}

sol::object runtimeTypeMetadata(sol::state_view lua,
                                const sol::table& classType) {
    const sol::object native =
        classType.raw_get<sol::object>("__runtimeMetadata");
    if (native.is<sol::table>()) {
        return native;
    }
    std::optional<RuntimeClassIdentity> identity =
        resolveRuntimeClassIdentity(lua, sol::make_object(lua, classType));
    if (!identity.has_value() || identity->module.empty()) {
        return nilObject(lua);
    }
    const std::string metadataModule = identity->module + "_meta";
    const sol::object rawPackage =
        lua.globals().raw_get<sol::object>("package");
    if (!rawPackage.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::object rawSearchPath =
        rawPackage.as<sol::table>().raw_get<sol::object>("searchpath");
    const sol::object rawPath =
        rawPackage.as<sol::table>().raw_get<sol::object>("path");
    if (!rawSearchPath.is<sol::protected_function>() ||
        !rawPath.is<std::string>()) {
        return nilObject(lua);
    }
    sol::protected_function searchPath =
        rawSearchPath.as<sol::protected_function>();
    sol::protected_function_result searched =
        searchPath(metadataModule, rawPath);
    const sol::object found = checkedResult(lua, searched);
    if (!found.is<std::string>()) {
        return nilObject(lua);
    }
    const sol::table metadata = requireLuaTable(lua, metadataModule.c_str());
    std::string typeName = identity->type;
    if (typeName.empty() && identity->direct) {
        for (const auto& entry : metadata) {
            if (!entry.first.is<std::string>() ||
                !entry.second.is<sol::table>()) {
                continue;
            }
            if (!typeName.empty()) {
                throw std::runtime_error(
                    "Metadata module for directly returned class must contain "
                    "one "
                    "type: " +
                    metadataModule);
            }
            typeName = entry.first.as<std::string>();
        }
        if (typeName.empty()) {
            throw std::runtime_error(
                "Metadata module for directly returned class contains no "
                "type: " +
                metadataModule);
        }
        identity->descriptor.raw_set("type", typeName);
    }
    return typeName.empty() ? nilObject(lua)
                            : metadata.raw_get<sol::object>(typeName);
}

void clearRuntimeServiceCaches(sol::state_view lua) {
    clearBlueprintRuntimeCaches(lua);
    lua.registry().raw_set(CLASS_IDENTITY_CACHE_KEY, sol::lua_nil);
    lua.registry().raw_set(CLASS_TYPE_METADATA_CACHE_KEY, sol::lua_nil);
    lua.registry().raw_set(ATTR_METADATA_CACHE_KEY, sol::lua_nil);
    clearRuntimeCommonCaches(lua);
    clearNodeGraphRuntimeCaches(lua);
}

}  // namespace ludork::engine::runtime_detail
