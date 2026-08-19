#include "Native/NativeRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"

#include <ClassServices.hpp>
#include <sol2/sol.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::standard::class_runtime::detail {

bool isNativeType(sol::state_view lua, const sol::table& value) {
    return !isClass(value) && typeInfoOf(lua, value).is<sol::table>();
}

std::string nativeTypeName(sol::state_view lua, const sol::table& nativeType) {
    const sol::object rawTypeInfo = typeInfoOf(lua, nativeType);
    if (!rawTypeInfo.is<sol::table>()) {
        throw std::invalid_argument(
            "Native class is missing binding type information");
    }
    const sol::object rawName =
        rawTypeInfo.as<sol::table>().raw_get<sol::object>("name");
    if (!rawName.is<std::string>()) {
        throw std::invalid_argument(
            "Native class is missing its qualified type name");
    }
    return rawName.as<std::string>();
}

sol::object nativeTypeDefinition(sol::state_view lua,
                                 const sol::table& nativeType,
                                 const sol::object& key) {
    const std::string registryName = "sol." + nativeTypeName(lua, nativeType);
    const sol::object rawMetatable =
        lua.registry().raw_get<sol::object>(registryName);
    if (!rawMetatable.is<sol::table>()) {
        return nilObject(lua);
    }
    return rawMetatable.as<sol::table>().raw_get<sol::object>(key);
}

bool nativeTypeDeclaresProperty(const sol::table& nativeType,
                                const sol::object& key) {
    if (!key.is<std::string>()) {
        return false;
    }
    const sol::object rawProperties =
        nativeType.raw_get<sol::object>("__nativeProperties");
    if (!rawProperties.is<sol::table>()) {
        return false;
    }
    const std::string name = key.as<std::string>();
    const sol::table properties = rawProperties.as<sol::table>();
    for (std::size_t index = 1; index <= properties.size(); ++index) {
        const sol::object rawName = properties.raw_get<sol::object>(index);
        if (rawName.is<std::string>() && rawName.as<std::string>() == name) {
            return true;
        }
    }
    return false;
}

sol::object nativeClassDefaultResolverKey(sol::state_view lua) {
    return sol::make_object(lua, sol::lightuserdata_value(static_cast<void*>(
                                     &nativeClassDefaultResolverKeyStorage)));
}

namespace {

sol::object resolveNativeClassDefault(sol::state_view lua,
                                      sol::table nativeType,
                                      const sol::object& key,
                                      sol::table defaults,
                                      const sol::object& value) {
    const sol::object rawResolvedDefaults =
        nativeType.raw_get<sol::object>(NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD);
    sol::table resolvedDefaults = rawResolvedDefaults.is<sol::table>()
                                      ? rawResolvedDefaults.as<sol::table>()
                                      : lua.create_table();
    if (!rawResolvedDefaults.is<sol::table>()) {
        nativeType.raw_set(NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD,
                           resolvedDefaults);
    }
    const sol::object rawResolved = resolvedDefaults.raw_get<sol::object>(key);
    if (rawResolved.is<bool>() && rawResolved.as<bool>()) {
        return value;
    }
    const sol::object rawMetadata =
        nativeType.raw_get<sol::object>("__runtimeMetadata");
    if (!rawMetadata.is<sol::table>()) {
        return value;
    }
    const sol::object rawFieldMetadata =
        rawMetadata.as<sol::table>().raw_get<sol::object>(key);
    if (!rawFieldMetadata.is<sol::table>()) {
        return value;
    }
    const sol::object rawResolver =
        lua.registry().raw_get<sol::object>(nativeClassDefaultResolverKey(lua));
    if (!rawResolver.is<sol::protected_function>()) {
        return value;
    }
    sol::protected_function resolver =
        rawResolver.as<sol::protected_function>();
    const sol::object rawModule =
        rawMetadata.as<sol::table>().raw_get<sol::object>("module");
    sol::protected_function_result result =
        resolver(value, rawFieldMetadata.as<sol::table>(), rawModule);
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(error.what());
    }
    if (result.return_count() == 0) {
        throw std::runtime_error(
            "Native class default resolver returned no value");
    }
    const sol::object resolved = result.get<sol::object>();
    defaults.raw_set(key, resolved);
    resolvedDefaults.raw_set(key, true);
    return resolved;
}

}  // namespace

bool nativeClassProperty(sol::state_view lua, const sol::table& nativeType,
                         const sol::object& key, sol::object& value) {
    const sol::table mro = getMro(lua, nativeType);
    sol::object rawOverride = nilObject(lua);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawType = mro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table current = rawType.as<sol::table>();
        if (rawOverride.get_type() == sol::type::lua_nil) {
            const sol::object rawValue = current.raw_get<sol::object>(key);
            if (rawValue.valid() && rawValue.get_type() != sol::type::lua_nil) {
                rawOverride = rawValue;
            }
        }
        if (!nativeTypeDeclaresProperty(current, key)) {
            continue;
        }
        if (rawOverride.get_type() != sol::type::lua_nil) {
            value = rawOverride;
            return true;
        }
        const sol::object rawDefaults =
            current.raw_get<sol::object>("__classDefaults");
        if (rawDefaults.is<sol::table>()) {
            sol::table defaults = rawDefaults.as<sol::table>();
            value = defaults.raw_get<sol::object>(key);
            if (value.valid() && value.get_type() != sol::type::lua_nil) {
                value = resolveNativeClassDefault(lua, current, key, defaults,
                                                  value);
            }
        } else {
            value = nilObject(lua);
        }
        if (!value.valid()) {
            value = nilObject(lua);
        }
        return true;
    }
    return false;
}

namespace {

sol::object nativeClassIndex(sol::table nativeType, sol::object key,
                             sol::this_state state) {
    sol::state_view lua(state);
    sol::object value = nilObject(lua);
    if (nativeClassProperty(lua, nativeType, key, value)) {
        return value;
    }
    const sol::table metatable = class_native::getObjectMetatable(
        lua, sol::make_object(lua, nativeType));
    const sol::object original =
        metatable.raw_get<sol::object>(NATIVE_CLASS_INDEX_FIELD);
    if (original.is<sol::protected_function>()) {
        sol::protected_function_result result =
            original.as<sol::protected_function>()(nativeType, key);
        if (!result.valid()) {
            const sol::error error = result;
            throw std::runtime_error(error.what());
        }
        return result.return_count() == 0 ? nilObject(lua)
                                          : result.get<sol::object>();
    }
    if (original.is<sol::table>()) {
        return protectedIndex(lua, original, key);
    }
    return nilObject(lua);
}

void nativeClassNewIndex(sol::table nativeType, sol::object key,
                         sol::object value, sol::this_state state) {
    sol::state_view lua(state);
    sol::object ignored = nilObject(lua);
    if (nativeClassProperty(lua, nativeType, key, ignored)) {
        nativeType.raw_set(key, value);
        invalidateClassLookup(lua, nativeType);
        return;
    }
    const sol::table metatable = class_native::getObjectMetatable(
        lua, sol::make_object(lua, nativeType));
    const sol::object original =
        metatable.raw_get<sol::object>(NATIVE_CLASS_NEW_INDEX_FIELD);
    if (original.is<sol::protected_function>()) {
        sol::protected_function_result result =
            original.as<sol::protected_function>()(nativeType, key, value);
        if (!result.valid()) {
            const sol::error error = result;
            throw std::runtime_error(error.what());
        }
        return;
    }
    if (original.is<sol::table>()) {
        original.as<sol::table>().raw_set(key, value);
        return;
    }
    nativeType.raw_set(key, value);
}

}  // namespace

bool nativeFallbackMemberEligible(const sol::object& key) {
    if (!key.is<std::string>()) {
        return true;
    }
    const std::string name = key.as<std::string>();
    return !name.starts_with("__") && name != "class_check" &&
           name != "class_cast";
}

std::vector<sol::table> nativeRoots(sol::state_view lua,
                                    const sol::table& classTable) {
    std::vector<sol::table> result;
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        bool covered = false;
        for (const sol::table& root : result) {
            if (derivesFrom(lua, root, nativeType)) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            result.push_back(nativeType);
        }
    }
    return result;
}

}  // namespace ludork::standard::class_runtime::detail

namespace ludork::standard::class_runtime {

void registerNativeClass(sol::table nativeType, const sol::table& metadata) {
    using namespace ludork::standard::class_runtime::detail;
    sol::state_view lua(nativeType.lua_state());
    sol::table defaults = lua.create_table();
    const sol::object rawAttrs = metadata.raw_get<sol::object>("attrs");
    if (rawAttrs.is<sol::table>()) {
        const sol::table attrs = rawAttrs.as<sol::table>();
        for (std::size_t index = 1; index <= attrs.size(); ++index) {
            const sol::object rawName = attrs.raw_get<sol::object>(index);
            if (!rawName.is<std::string>()) {
                continue;
            }
            const sol::object rawField =
                metadata.raw_get<sol::object>(rawName.as<std::string>());
            if (!rawField.is<sol::table>()) {
                continue;
            }
            const sol::object rawDefault =
                rawField.as<sol::table>().raw_get<sol::object>("default");
            if (!rawDefault.valid() ||
                rawDefault.get_type() == sol::type::lua_nil) {
                continue;
            }
            const sol::object value =
                class_runtime::clonePlainData(lua, rawDefault);
            defaults.raw_set(rawName, value);
        }
    }
    nativeType.raw_set("__classDefaults", defaults);
    nativeType.raw_set(NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD,
                       lua.create_table());

    sol::table metatable = class_native::getObjectMetatable(
        lua, sol::make_object(lua, nativeType));
    const sol::object rawGuard =
        metatable.raw_get<sol::object>(NATIVE_CLASS_GUARD_FIELD);
    if (rawGuard.is<bool>() && rawGuard.as<bool>()) {
        return;
    }
    metatable.raw_set(NATIVE_CLASS_INDEX_FIELD,
                      metatable.raw_get<sol::object>("__index"));
    metatable.raw_set(NATIVE_CLASS_NEW_INDEX_FIELD,
                      metatable.raw_get<sol::object>("__newindex"));
    metatable.set_function("__index", &nativeClassIndex);
    metatable.set_function("__newindex", &nativeClassNewIndex);
    metatable.raw_set(NATIVE_CLASS_GUARD_FIELD, true);
}

void registerNativeClassDefaultResolver(
    sol::state_view lua, const sol::protected_function& callback) {
    lua.registry().raw_set(detail::nativeClassDefaultResolverKey(lua),
                           callback);
}

void unregisterNativeClassDefaultResolver(sol::state_view lua) {
    lua.registry().raw_set(detail::nativeClassDefaultResolverKey(lua),
                           sol::lua_nil);
}

}  // namespace ludork::standard::class_runtime
