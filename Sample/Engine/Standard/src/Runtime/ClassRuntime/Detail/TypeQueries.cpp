#include "Detail/TypeQueries.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

namespace ludork::standard::class_runtime::detail {

bool isNativeType(sol::state_view lua, const sol::table& value) {
    return !isClass(value) && typeInfoOf(lua, value).is<sol::table>();
}

bool isCompositeInstance(sol::state_view lua, const sol::object& instance) {
    if (!instance.is<sol::userdata>()) {
        return false;
    }
    const sol::object marker =
        class_native::getObjectMetatable(lua, instance)
            .raw_get<sol::object>(protocol::COMPOSITE_MARKER_FIELD);
    return marker.is<bool>() && marker.as<bool>();
}

sol::object scriptClassOf(sol::state_view lua, const sol::object& value) {
    if (value.get_type() == sol::type::table) {
        const sol::table tableValue = value.as<sol::table>();
        const sol::object rawClass = tableValue.raw_get<sol::object>("__class");
        if (rawClass.is<sol::table>()) {
            return rawClass;
        }
        const sol::table metatable =
            class_native::getObjectMetatable(lua, value);
        if (isClass(metatable)) {
            return sol::make_object(lua, metatable);
        }
    } else if (value.get_type() == sol::type::userdata) {
        lua_State* state = lua.lua_state();
        value.push();
        const int valueIndex = lua_absindex(state, -1);
        if (lua_getiuservalue(state, valueIndex, 1) == LUA_TTABLE) {
            lua_getfield(state, -1, "__class");
            const sol::object rawClass =
                sol::stack::get<sol::object>(state, -1);
            lua_pop(state, 3);
            if (rawClass.is<sol::table>()) {
                return rawClass;
            }
        } else {
            lua_pop(state, 2);
        }
    }
    return nilObject(lua);
}

sol::object typeInfoOf(sol::state_view lua, const sol::table& nativeType) {
    return class_native::getObjectMetatable(lua,
                                            sol::make_object(lua, nativeType))
        .raw_get<sol::object>("__type");
}

namespace {

sol::object findNativeTypeInNamespace(sol::state_view lua,
                                      const sol::table& nameSpace,
                                      const sol::table& targetTypeInfo) {
    for (const auto& entry : nameSpace) {
        const sol::object candidate = entry.second;
        if (!candidate.is<sol::table>()) {
            continue;
        }
        const sol::object candidateTypeInfo =
            typeInfoOf(lua, candidate.as<sol::table>());
        if (candidateTypeInfo.is<sol::table>() &&
            objectsRawEqual(candidateTypeInfo.as<sol::table>(),
                            targetTypeInfo)) {
            return candidate;
        }
    }
    return nilObject(lua);
}

}  // namespace

sol::object nativeTypeOf(sol::state_view lua, const sol::object& value) {
    if (value.get_type() != sol::type::userdata) {
        return nilObject(lua);
    }
    const sol::object rawTypeInfo = class_native::getObjectMetatable(lua, value)
                                        .raw_get<sol::object>("__type");
    if (!rawTypeInfo.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::table typeInfo = rawTypeInfo.as<sol::table>();
    sol::table cache = registryTable(lua, NATIVE_TYPE_CACHE_KEY);
    const sol::object cached = cache.raw_get<sol::object>(typeInfo);
    if (cached.is<sol::table>()) {
        return cached;
    }
    const sol::table globals = lua.globals();
    sol::object result = findNativeTypeInNamespace(lua, globals, typeInfo);
    if (!result.is<sol::table>()) {
        for (const auto& entry : globals) {
            const sol::object nameSpace = entry.second;
            if (!nameSpace.is<sol::table>()) {
                continue;
            }
            const sol::table tableValue = nameSpace.as<sol::table>();
            if (objectsRawEqual(tableValue, globals)) {
                continue;
            }
            result = findNativeTypeInNamespace(lua, tableValue, typeInfo);
            if (result.is<sol::table>()) {
                break;
            }
        }
    }
    if (result.is<sol::table>()) {
        cache.raw_set(typeInfo, result);
    }
    return result;
}

sol::object actualClassOf(sol::state_view lua, const sol::object& value) {
    if (value.is<sol::table>() && isClass(value.as<sol::table>())) {
        return value;
    }
    sol::object result = scriptClassOf(lua, value);
    if (result.is<sol::table>()) {
        return result;
    }
    return nativeTypeOf(lua, value);
}

}  // namespace ludork::standard::class_runtime::detail
