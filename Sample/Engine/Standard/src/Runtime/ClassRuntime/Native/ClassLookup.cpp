#include "Native/NativeRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/TypedFields.hpp"

#include <sol2/sol.hpp>

#include <cstddef>

namespace ludork::standard::class_runtime::detail {

sol::object rawMember(sol::state_view lua, const sol::table& type,
                      const sol::object& key) {
    sol::object result = type.raw_get<sol::object>(key);
    if ((!result.valid() || result.get_type() == sol::type::lua_nil) &&
        !isClass(type)) {
        if (nativeClassProperty(lua, type, key, result)) {
            return result;
        }
        const sol::object rawIndex =
            class_native::getObjectMetatable(lua, sol::make_object(lua, type))
                .raw_get<sol::object>("__index");
        if (rawIndex.is<sol::protected_function>()) {
            sol::protected_function_result indexed =
                rawIndex.as<sol::protected_function>()(type, key);
            if (indexed.valid()) {
                result = indexed.get<sol::object>();
            }
        }
    }
    return result.valid() ? result : nilObject(lua);
}

sol::object findInClass(sol::state_view lua, const sol::table& classTable,
                        const sol::object& key, bool includeClass) {
    sol::table owners = classLookupOwners(
        lua, classTable, includeClass ? "members" : "baseMembers");
    const sol::object rawOwner = owners.raw_get<sol::object>(key);
    if (rawOwner.is<sol::table>()) {
        const sol::object cached =
            rawMember(lua, rawOwner.as<sol::table>(), key);
        if ((cached.valid() && cached.get_type() != sol::type::lua_nil) ||
            hasExplicitNilField(lua, rawOwner, key)) {
            return cached;
        }
        owners.raw_set(key, sol::lua_nil);
    }
    const sol::table mro = getMro(lua, classTable);
    const std::size_t start = includeClass ? 1 : 2;
    for (std::size_t index = start; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::object result =
            rawMember(lua, rawType.as<sol::table>(), key);
        if ((result.valid() && result.get_type() != sol::type::lua_nil) ||
            hasExplicitNilField(lua, rawType, key)) {
            owners.raw_set(key, rawType);
            return result;
        }
    }
    return nilObject(lua);
}

}  // namespace ludork::standard::class_runtime::detail
