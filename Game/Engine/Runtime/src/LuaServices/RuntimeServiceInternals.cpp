#include "RuntimeServiceInternals.hpp"
#include <ClassRuntimeProtocol.hpp>

#include "RuntimeBindingTraits.hpp"
#include "RuntimeClassIdentity.hpp"
#include "Metadata/ConfigVarReferences.hpp"
#include <ClassServices.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <LuaGlue/LuaGlue.hpp>

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
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::runtime::detail {

constexpr const char* CLASS_IDENTITY_CACHE_KEY =
    "Ludork.Runtime.classIdentityCache";
constexpr const char* CLASS_TYPE_METADATA_CACHE_KEY =
    "Ludork.Runtime.classTypeMetadataCache";
constexpr const char* ATTR_METADATA_CACHE_KEY =
    "Ludork.Runtime.attrMetadataCache";

RuntimeClassIdentity classIdentityFromDescriptor(
    const lua_glue::Table& descriptor) {
    const lua_glue::Object rawModule =
        descriptor.raw_get<lua_glue::Object>("module");
    const lua_glue::Object rawType =
        descriptor.raw_get<lua_glue::Object>("type");
    return {
        descriptor,
        rawModule.is<std::string>() ? rawModule.as<std::string>()
                                    : std::string(),
        rawType.is<std::string>() ? rawType.as<std::string>() : std::string(),
        rawBool(descriptor, "direct")};
}

std::optional<RuntimeClassIdentity> resolveRuntimeClassIdentity(
    lua_glue::StateView lua, const lua_glue::Object& classReference) {
    if (!classReference.is<lua_glue::Table>()) {
        return std::nullopt;
    }
    const lua_glue::Table classTable = classReference.as<lua_glue::Table>();
    lua_glue::Table cache = registryTable(lua, CLASS_IDENTITY_CACHE_KEY, "k");
    const lua_glue::Object cached =
        cache.raw_get<lua_glue::Object>(classReference);
    if (cached.is<lua_glue::Table>()) {
        return classIdentityFromDescriptor(cached.as<lua_glue::Table>());
    }

    const lua_glue::Object explicitModule = classTable.raw_get<
        lua_glue::Object>(
        ludork::standard::class_runtime::protocol::CLASS_METADATA_MODULE_FIELD);
    if (explicitModule.is<std::string>() &&
        !explicitModule.as<std::string>().empty()) {
        lua_glue::Table descriptor = lua.create_table();
        descriptor.raw_set("module", explicitModule);
        descriptor.raw_set("direct", false);
        cache.raw_set(classReference, descriptor);
        return classIdentityFromDescriptor(descriptor);
    }

    const lua_glue::Object rawPackage =
        lua.globals().raw_get<lua_glue::Object>("package");
    if (!rawPackage.is<lua_glue::Table>()) {
        return std::nullopt;
    }
    const lua_glue::Object rawLoaded =
        rawPackage.as<lua_glue::Table>().raw_get<lua_glue::Object>("loaded");
    if (!rawLoaded.is<lua_glue::Table>()) {
        return std::nullopt;
    }
    const lua_glue::Table loaded = rawLoaded.as<lua_glue::Table>();

    lua_State* state = lua.lua_state();
    struct StackRestore {
        lua_State* state;
        int top;
        ~StackRestore() {
            lua_settop(state, top);
        }
    } restore{state, lua_gettop(state)};
    loaded.push(lua.lua_state());
    const int loadedIndex = lua_gettop(state);
    classReference.push(lua.lua_state());
    const int classIndex = lua_gettop(state);
    std::vector<std::string> directModules;
    lua_pushnil(state);
    while (lua_next(state, loadedIndex) != 0) {
        if (lua_type(state, -2) == LUA_TSTRING &&
            lua_rawequal(state, -1, classIndex)) {
            std::size_t size = 0;
            const char* name = lua_tolstring(state, -2, &size);
            directModules.emplace_back(name, size);
        }
        lua_pop(state, 1);
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
        lua_glue::Table descriptor = lua.create_table();
        descriptor.raw_set("module", directModules.front());
        descriptor.raw_set("direct", true);
        cache.raw_set(classReference, descriptor);
        return classIdentityFromDescriptor(descriptor);
    }

    std::vector<std::pair<std::string, std::string>> exports;
    lua_pushnil(state);
    while (lua_next(state, loadedIndex) != 0) {
        if (lua_type(state, -2) == LUA_TSTRING && lua_istable(state, -1)) {
            const int moduleIndex = lua_gettop(state);
            lua_pushnil(state);
            while (lua_next(state, moduleIndex) != 0) {
                if (lua_type(state, -2) == LUA_TSTRING &&
                    lua_rawequal(state, -1, classIndex)) {
                    std::size_t exportSize = 0;
                    const char* exportName =
                        lua_tolstring(state, -2, &exportSize);
                    const std::string_view name(exportName, exportSize);
                    if (!name.starts_with("__") &&
                        name != "_hasImplementationOwner") {
                        std::size_t moduleSize = 0;
                        const char* moduleName =
                            lua_tolstring(state, moduleIndex - 1, &moduleSize);
                        exports.emplace_back(
                            std::string(moduleName, moduleSize),
                            std::string(name));
                    }
                }
                lua_pop(state, 1);
            }
        }
        lua_pop(state, 1);
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

    lua_glue::Table descriptor = lua.create_table();
    descriptor.raw_set("module", exports.front().first);
    descriptor.raw_set("type", exports.front().second);
    descriptor.raw_set("direct", false);
    cache.raw_set(classReference, descriptor);
    return classIdentityFromDescriptor(descriptor);
}

lua_glue::Object findRuntimeClassModule(
    lua_glue::StateView lua, const lua_glue::Object& classReference) {
    const std::optional<RuntimeClassIdentity> identity =
        resolveRuntimeClassIdentity(lua, classReference);
    return identity.has_value() && !identity->module.empty()
               ? lua_glue::MakeObject(lua, identity->module)
               : nilObject(lua);
}

lua_glue::Object syntheticRuntimeMetadata(lua_glue::StateView lua,
                                          const lua_glue::Table& classTable) {
    const lua_glue::Object native = classTable.raw_get<lua_glue::Object>(
        ludork::standard::class_runtime::protocol::RUNTIME_METADATA_FIELD);
    if (native.is<lua_glue::Table>()) {
        return native;
    }
    const lua_glue::Object rawTypes =
        classTable.raw_get<lua_glue::Object>("__types");
    const lua_glue::Object rawMeta =
        classTable.raw_get<lua_glue::Object>("__runtimeMeta");
    if (!rawTypes.is<lua_glue::Table>() && !rawMeta.is<lua_glue::Table>()) {
        return nilObject(lua);
    }

    lua_glue::Table metadata = lua.create_table();
    lua_glue::Table attrs = lua.create_table();
    std::size_t index = 1;
    if (rawTypes.is<lua_glue::Table>()) {
        for (const auto& entry : rawTypes.as<lua_glue::Table>()) {
            if (!entry.first.is<std::string>()) {
                continue;
            }
            const std::string name = entry.first.as<std::string>();
            attrs.raw_set(index++, name);
            lua_glue::Table member = lua.create_table();
            member.raw_set("type", entry.second);
            metadata.raw_set(name, member);
        }
    }
    metadata.raw_set("attrs", attrs);
    if (rawMeta.is<lua_glue::Table>()) {
        metadata.raw_set("Meta", rawMeta);
    }
    return lua_glue::MakeObject(lua, metadata);
}

namespace {

bool runtimeModuleExists(lua_glue::StateView lua,
                         const lua_glue::Table& package,
                         const std::string& moduleName) {
    for (const char* field : {"loaded", "preload"}) {
        const lua_glue::Object rawModules =
            package.raw_get<lua_glue::Object>(field);
        if (!rawModules.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Object module =
            rawModules.as<lua_glue::Table>().raw_get<lua_glue::Object>(
                moduleName);
        if (module.valid() && module.get_type() != lua_glue::Type::Nil) {
            return true;
        }
    }
    const lua_glue::Object rawSearch =
        package.raw_get<lua_glue::Object>("searchpath");
    if (!rawSearch.is<lua_glue::Function>()) {
        return false;
    }
    lua_glue::Function search = rawSearch.as<lua_glue::Function>();
    for (const char* field : {"path", "cpath"}) {
        const lua_glue::Object rawPath =
            package.raw_get<lua_glue::Object>(field);
        if (!rawPath.is<std::string>()) {
            continue;
        }
        lua_glue::CallResult result = search(moduleName, rawPath);
        if (result.valid() && result.return_count() > 0) {
            const lua_glue::Object found = result.get<lua_glue::Object>();
            if (found.is<std::string>()) {
                return true;
            }
        }
    }
    return false;
}

lua_glue::Object moduleTypeMetadata(lua_glue::StateView lua,
                                    const RuntimeClassIdentity& identity) {
    if (identity.module.empty()) {
        return nilObject(lua);
    }
    const std::string metadataModule = identity.module + "_meta";
    const lua_glue::Object rawPackage =
        lua.globals().raw_get<lua_glue::Object>("package");
    if (!rawPackage.is<lua_glue::Table>()) {
        return nilObject(lua);
    }
    const lua_glue::Table package = rawPackage.as<lua_glue::Table>();
    if (!runtimeModuleExists(lua, package, metadataModule)) {
        return nilObject(lua);
    }
    const lua_glue::Table metadata =
        requireLuaTable(lua, metadataModule.c_str());
    std::string typeName = identity.type;
    if (typeName.empty() && identity.direct) {
        for (const auto& entry : metadata) {
            if (!entry.first.is<std::string>() ||
                !entry.second.is<lua_glue::Table>()) {
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
        lua_glue::Table descriptor = identity.descriptor;
        descriptor.raw_set("type", typeName);
    }
    return typeName.empty() ? nilObject(lua)
                            : metadata.raw_get<lua_glue::Object>(typeName);
}

}  // namespace

lua_glue::Table runtimeClassTypeDescriptor(
    lua_glue::StateView lua, const lua_glue::Table& classReference) {
    lua_glue::Table cache =
        registryTable(lua, CLASS_TYPE_METADATA_CACHE_KEY, "k");
    const lua_glue::Object cached =
        cache.raw_get<lua_glue::Object>(classReference);
    if (cached.is<lua_glue::Table>()) {
        return cached.as<lua_glue::Table>();
    }
    lua_glue::Object metadata = syntheticRuntimeMetadata(lua, classReference);
    const std::optional<RuntimeClassIdentity> identity =
        resolveRuntimeClassIdentity(lua,
                                    lua_glue::MakeObject(lua, classReference));
    if (!metadata.is<lua_glue::Table>() && identity.has_value()) {
        metadata = moduleTypeMetadata(lua, *identity);
    }
    lua_glue::Table descriptor = lua.create_table();
    descriptor.raw_set("hasMetadata", metadata.is<lua_glue::Table>());
    if (metadata.is<lua_glue::Table>()) {
        descriptor.raw_set("metadata", metadata);
    }
    const lua_glue::Object nativeMetadata =
        classReference.raw_get<lua_glue::Object>(
            ludork::standard::class_runtime::protocol::RUNTIME_METADATA_FIELD);
    if (nativeMetadata.is<lua_glue::Table>() ||
        (!classReference.raw_get<lua_glue::Object>("__types")
              .is<lua_glue::Table>() &&
         !classReference.raw_get<lua_glue::Object>("__runtimeMeta")
              .is<lua_glue::Table>())) {
        descriptor.raw_set("runtimeMetadataResolved", true);
        descriptor.raw_set("runtimeMetadata", metadata);
    }
    if (identity.has_value() && !identity->module.empty()) {
        descriptor.raw_set("module", identity->module);
        descriptor.raw_set("identity", identity->descriptor);
    }
    cache.raw_set(classReference, descriptor);
    return descriptor;
}

lua_glue::Table collectRuntimeAttrMetadata(lua_glue::StateView lua,
                                           const lua_glue::Table& owner) {
    lua_glue::Table cache = registryTable(lua, ATTR_METADATA_CACHE_KEY, "k");
    const lua_glue::Object ownerObject = lua_glue::MakeObject(lua, owner);
    const lua_glue::Object cached =
        cache.raw_get<lua_glue::Object>(ownerObject);
    if (cached.is<lua_glue::Table>()) {
        return cached.as<lua_glue::Table>();
    }
    lua_glue::Table result = lua.create_table();
    const std::vector<lua_glue::Table> mro = runtimeClassMro(lua, owner);
    for (auto current = mro.rbegin(); current != mro.rend(); ++current) {
        const lua_glue::Table classDescriptor =
            runtimeClassTypeDescriptor(lua, *current);
        const lua_glue::Object rawMetadata =
            classDescriptor.raw_get<lua_glue::Object>("metadata");
        if (!rawMetadata.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table metadata = rawMetadata.as<lua_glue::Table>();
        const lua_glue::Object rawAttrs =
            metadata.raw_get<lua_glue::Object>("attrs");
        if (!rawAttrs.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Object module =
            classDescriptor.raw_get<lua_glue::Object>("module");
        const lua_glue::Table attrs = rawAttrs.as<lua_glue::Table>();
        for (std::size_t index = 1; index <= attrs.size(); ++index) {
            const lua_glue::Object rawName =
                attrs.raw_get<lua_glue::Object>(index);
            if (!rawName.is<std::string>()) {
                continue;
            }
            const std::string name = rawName.as<std::string>();
            const lua_glue::Object rawMember =
                metadata.raw_get<lua_glue::Object>(name);
            if (!rawMember.is<lua_glue::Table>()) {
                continue;
            }
            const lua_glue::Table member = rawMember.as<lua_glue::Table>();
            const lua_glue::Object type =
                member.raw_get<lua_glue::Object>("type");
            if (type.get_type() == lua_glue::Type::Nil) {
                continue;
            }
            lua_glue::Table descriptor = lua.create_table();
            descriptor.raw_set("type", type);
            const lua_glue::Object component =
                member.raw_get<lua_glue::Object>("component");
            descriptor.raw_set("component",
                               component.is<bool>() && component.as<bool>());
            const lua_glue::Object declaredModule =
                member.raw_get<lua_glue::Object>("module");
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
        const lua_glue::Object rawTypes =
            current->raw_get<lua_glue::Object>("__types");
        if (!rawTypes.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Object module =
            runtimeClassTypeDescriptor(lua, *current)
                .raw_get<lua_glue::Object>("module");
        for (const auto& entry : rawTypes.as<lua_glue::Table>()) {
            if (!entry.first.is<std::string>()) {
                continue;
            }
            const std::string name = entry.first.as<std::string>();
            const lua_glue::Object existing =
                result.raw_get<lua_glue::Object>(name);
            lua_glue::Table descriptor = lua.create_table();
            descriptor.raw_set("type", entry.second);
            if (existing.is<lua_glue::Table>()) {
                const lua_glue::Table previous = existing.as<lua_glue::Table>();
                const lua_glue::Object component =
                    previous.raw_get<lua_glue::Object>("component");
                descriptor.raw_set(
                    "component", component.is<bool>() && component.as<bool>());
                const lua_glue::Object previousModule =
                    previous.raw_get<lua_glue::Object>("module");
                if (previousModule.is<std::string>()) {
                    descriptor.raw_set("module", previousModule);
                }
                const lua_glue::Object metadata =
                    previous.raw_get<lua_glue::Object>("metadata");
                if (metadata.get_type() != lua_glue::Type::Nil) {
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

std::pair<lua_glue::Object, lua_glue::Object> resolveRuntimeConfigVar(
    lua_glue::StateView lua, const lua_glue::Object& owner,
    const lua_glue::Object& rawName) {
    if (!rawName.is<std::string>()) {
        return {nilObject(lua), nilObject(lua)};
    }
    lua_glue::Object rawClass = owner;
    if (!rawClass.is<lua_glue::Table>()) {
        rawClass = ludork::standard::class_runtime::typeOf(lua, owner);
    }
    if (!rawClass.is<lua_glue::Table>()) {
        return {nilObject(lua), nilObject(lua)};
    }
    const std::string name = rawName.as<std::string>();
    for (const lua_glue::Table& current :
         runtimeClassMro(lua, rawClass.as<lua_glue::Table>())) {
        const lua_glue::Object metadata =
            runtimeClassTypeDescriptor(lua, current)
                .raw_get<lua_glue::Object>("metadata");
        if (!metadata.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Object meta =
            metadata.as<lua_glue::Table>().raw_get<lua_glue::Object>("Meta");
        if (!meta.is<lua_glue::Table>()) {
            continue;
        }
        const RuntimeValue::Map references = parseConfigVarReferences(
            ludork::runtime::binding::readLuaValue<RuntimeValue>(meta));
        const auto iterator = references.find(name);
        if (iterator == references.end()) {
            continue;
        }
        std::optional<RuntimeArrayView> reference =
            RuntimeValueView(iterator->second).array();
        if (!reference || reference->size() < 2) {
            continue;
        }
        return {
            ludork::runtime::binding::writeLuaValue(lua,
                                                    (*reference)[0].toValue()),
            ludork::runtime::binding::writeLuaValue(lua,
                                                    (*reference)[1].toValue()),
        };
    }
    return {nilObject(lua), nilObject(lua)};
}

std::pair<lua_glue::Object, lua_glue::Object> resolveRuntimeMemberMetadata(
    lua_glue::StateView lua, const lua_glue::Object& owner,
    const lua_glue::Object& rawName) {
    if (!rawName.is<std::string>()) {
        return {nilObject(lua), nilObject(lua)};
    }
    const std::string name = rawName.as<std::string>();
    const lua_glue::Table mro =
        ludork::standard::class_runtime::getMroCopy(lua, owner);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const lua_glue::Object rawClass = mro.raw_get<lua_glue::Object>(index);
        if (!rawClass.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table classDescriptor =
            runtimeClassTypeDescriptor(lua, rawClass.as<lua_glue::Table>());
        const lua_glue::Object metadata =
            classDescriptor.raw_get<lua_glue::Object>("metadata");
        if (!metadata.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Object member =
            metadata.as<lua_glue::Table>().raw_get<lua_glue::Object>(name);
        if (!member.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Object module =
            classDescriptor.raw_get<lua_glue::Object>("module");
        return {member, module.valid() ? module : nilObject(lua)};
    }
    return {nilObject(lua), nilObject(lua)};
}

lua_glue::Object resolveRuntimePath(lua_glue::StateView lua,
                                    const lua_glue::Object& root,
                                    const std::string& path) {
    lua_glue::Object current = root;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t end = path.find('.', start);
        const std::string name = path.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (!current.is<lua_glue::Table>()) {
            return nilObject(lua);
        }
        current = current.as<lua_glue::Table>().get<lua_glue::Object>(name);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return current.valid() ? current : nilObject(lua);
}

std::optional<std::string> directRuntimeMetadataTypeName(
    lua_glue::StateView lua, const std::string& modulePath) {
    const std::string metadataModule = modulePath + "_meta";
    const lua_glue::Object rawPackage =
        lua.globals().raw_get<lua_glue::Object>("package");
    if (!rawPackage.is<lua_glue::Table>()) {
        return std::nullopt;
    }
    const lua_glue::Table package = rawPackage.as<lua_glue::Table>();
    if (!runtimeModuleExists(lua, package, metadataModule)) {
        return std::nullopt;
    }

    const lua_glue::Table metadata =
        requireLuaTable(lua, metadataModule.c_str());
    std::optional<std::string> result;
    for (const auto& entry : metadata) {
        if (!entry.first.is<std::string>() ||
            !entry.second.is<lua_glue::Table>()) {
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

lua_glue::Object requireRuntimeType(lua_glue::StateView lua,
                                    const std::string& modulePath,
                                    const std::string& typeName) {
    const lua_glue::Object rawRequire =
        lua.globals().raw_get<lua_glue::Object>("require");
    if (!rawRequire.is<lua_glue::Function>()) {
        return nilObject(lua);
    }
    lua_glue::Function require = rawRequire.as<lua_glue::Function>();
    lua_glue::CallResult loaded = require(modulePath);
    const lua_glue::Object module = checkedResult(lua, loaded);
    if (!module.is<lua_glue::Table>()) {
        return nilObject(lua);
    }
    const lua_glue::Table table = module.as<lua_glue::Table>();
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

lua_glue::Object resolveRuntimeMetadataType(
    lua_glue::StateView lua, const lua_glue::Object& typeReference,
    const lua_glue::Object& declaringModule) {
    if (typeReference.is<lua_glue::Table>()) {
        const lua_glue::Table reference = typeReference.as<lua_glue::Table>();
        const lua_glue::Object module = reference.raw_get<lua_glue::Object>(1);
        const lua_glue::Object name = reference.raw_get<lua_glue::Object>(2);
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
    const lua_glue::Object global =
        resolveRuntimePath(lua, lua_glue::MakeObject(lua, lua.globals()), name);
    if (global.get_type() != lua_glue::Type::Nil) {
        return global;
    }
    if (declaringModule.is<std::string>() &&
        name.find('.') == std::string::npos) {
        return requireRuntimeType(lua, declaringModule.as<std::string>(), name);
    }
    return nilObject(lua);
}

bool runtimeSequence(const lua_glue::Table& table,
                     std::vector<lua_glue::Object>& values) {
    const lua_glue::Object rawLength = table.raw_get<lua_glue::Object>("n");
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
        values.push_back(table.raw_get<lua_glue::Object>(index));
    }
    return true;
}

lua_glue::Object evaluateRuntimeExpression(
    lua_glue::StateView lua, const lua_glue::Object& value,
    const lua_glue::Object& rawEnvironment) {
    if (!value.is<std::string>()) {
        return value;
    }
    const std::string expression = value.as<std::string>();
    lua_glue::CallResult loaded = lua.load("return " + expression, "=(data)");
    if (!loaded.valid()) {
        return value;
    }
    lua_glue::Function function = loaded.get<lua_glue::Function>();
    lua_glue::Table environment = lua.create_table();
    lua_glue::Table environmentMetatable = lua.create_table();
    environmentMetatable.raw_set("__index", lua.globals());
    lua_glue::SetMetatable(environment, environmentMetatable);
    if (rawEnvironment.is<lua_glue::Table>()) {
        for (const auto& entry : rawEnvironment.as<lua_glue::Table>()) {
            environment.raw_set(entry.first, entry.second);
        }
    }
    {
        auto functionStack = lua_glue::PushGuard(function);
        environment.push(lua.lua_state());
        lua_setupvalue(lua.lua_state(), functionStack.index(), 1);
    }
    lua_glue::CallResult result = function();
    if (!result.valid()) {
        return value;
    }
    lua_glue::Object evaluated = result.return_count() == 0
                                     ? nilObject(lua)
                                     : result.get<lua_glue::Object>();
    static const std::regex bareIdentifier(R"(^\s*[A-Za-z_][A-Za-z0-9_]*\s*$)");
    static const std::regex nilLiteral(R"(^\s*nil\s*$)");
    if (evaluated.get_type() == lua_glue::Type::Nil &&
        std::regex_match(expression, bareIdentifier) &&
        !std::regex_match(expression, nilLiteral)) {
        return value;
    }
    return evaluated;
}

lua_glue::Object runtimeTypeMetadata(lua_glue::StateView lua,
                                     const lua_glue::Table& classType) {
    lua_glue::Table descriptor = runtimeClassTypeDescriptor(lua, classType);
    if (rawBool(descriptor, "runtimeMetadataResolved")) {
        return descriptor.raw_get<lua_glue::Object>("runtimeMetadata");
    }
    const lua_glue::Object rawIdentity =
        descriptor.raw_get<lua_glue::Object>("identity");
    const lua_glue::Object metadata =
        rawIdentity.is<lua_glue::Table>()
            ? moduleTypeMetadata(lua, classIdentityFromDescriptor(
                                          rawIdentity.as<lua_glue::Table>()))
            : nilObject(lua);
    descriptor.raw_set("runtimeMetadataResolved", true);
    descriptor.raw_set("runtimeMetadata", metadata);
    return metadata;
}

void clearRuntimeCaches(lua_glue::StateView lua) {
    lua.registry().raw_set(CLASS_IDENTITY_CACHE_KEY, lua_glue::nil);
    lua.registry().raw_set(CLASS_TYPE_METADATA_CACHE_KEY, lua_glue::nil);
    lua.registry().raw_set(ATTR_METADATA_CACHE_KEY, lua_glue::nil);
}

lua_glue::Object resolveRuntimeAttrValueType(lua_glue::StateView lua,
                                             const lua_glue::Object& rawOwner,
                                             const std::string& key) {
    lua_glue::Object valueType = ludork::runtime::detail::nilObject(lua);
    if (rawOwner.is<lua_glue::Table>()) {
        const lua_glue::Table metadata =
            ludork::runtime::detail::collectRuntimeAttrMetadata(
                lua, rawOwner.as<lua_glue::Table>());
        const lua_glue::Object descriptor =
            metadata.raw_get<lua_glue::Object>(key);
        if (descriptor.is<lua_glue::Table>()) {
            valueType =
                descriptor.as<lua_glue::Table>().raw_get<lua_glue::Object>(
                    "type");
        }
        if (!valueType.valid() || valueType.get_type() == lua_glue::Type::Nil) {
            const lua_glue::Object value =
                rawOwner.as<lua_glue::Table>().get<lua_glue::Object>(key);
            switch (value.get_type()) {
                case lua_glue::Type::Boolean:
                    valueType = lua_glue::MakeObject(lua, "bool");
                    break;
                case lua_glue::Type::Number: {
                    value.push(lua.lua_state());
                    const bool integer =
                        lua_isinteger(lua.lua_state(), -1) != 0;
                    lua_pop(lua.lua_state(), 1);
                    valueType =
                        lua_glue::MakeObject(lua, integer ? "int" : "float");
                    break;
                }
                case lua_glue::Type::String:
                    valueType = lua_glue::MakeObject(lua, "string");
                    break;
                case lua_glue::Type::Table:
                    valueType = lua_glue::MakeObject(lua, "table");
                    break;
                case lua_glue::Type::Userdata: {
                    const lua_glue::Table metatable =
                        ludork::runtime::detail::objectMetatable(lua, value);
                    const lua_glue::Object declared =
                        metatable.raw_get<lua_glue::Object>("__metadataType");
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
    if (!valueType.valid() || valueType.get_type() == lua_glue::Type::Nil) {
        valueType = lua_glue::MakeObject(lua, "any");
    }
    return valueType;
}

}  // namespace ludork::runtime::detail
