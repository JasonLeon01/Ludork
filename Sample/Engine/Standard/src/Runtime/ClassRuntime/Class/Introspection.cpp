#include "Class/ClassRuntimeInternals.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Native/NativeRuntime.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>

namespace ludork::standard::class_runtime::detail {

sol::table ownFields(sol::state_view lua, const sol::object& target) {
    if (target.is<sol::table>()) {
        return target.as<sol::table>();
    }
    if (target.get_type() == sol::type::userdata) {
        return class_native::getUserFields(lua, target, false);
    }
    return lua.create_table();
}

sol::object rawOwnField(sol::state_view lua, const sol::object& target,
                        const sol::object& key) {
    if (target.get_type() == sol::type::userdata) {
        lua_State* state = lua.lua_state();
        target.push();
        if (lua_getiuservalue(state, -1, 1) != LUA_TTABLE) {
            lua_pop(state, 2);
            return nilObject(lua);
        }
        key.push();
        lua_rawget(state, -2);
        sol::object result = sol::stack::get<sol::object>(state, -1);
        lua_pop(state, 3);
        return result;
    }
    if (target.is<sol::table>()) {
        return target.as<sol::table>().raw_get<sol::object>(key);
    }
    return nilObject(lua);
}

bool hasRawOwnField(sol::state_view lua, const sol::object& target,
                    const sol::object& key) {
    const sol::object value = rawOwnField(lua, target, key);
    return value.valid() && value.get_type() != sol::type::lua_nil;
}

sol::table ownKeyList(sol::state_view lua, const sol::object& target) {
    sol::table result = lua.create_table();
    for (const auto& entry : ownFields(lua, target)) {
        result.add(entry.first);
    }
    return result;
}

sol::table mroCopy(sol::state_view lua, const sol::object& value) {
    sol::object rawClass = value;
    if (!value.is<sol::table>() ||
        (!isClass(value.as<sol::table>()) &&
         !isNativeType(lua, value.as<sol::table>()))) {
        rawClass = actualClassOf(lua, value);
    }
    if (!rawClass.is<sol::table>()) {
        throw std::invalid_argument(
            "Class.getMro requires a class or class instance");
    }
    const sol::table mro = getMro(lua, rawClass.as<sol::table>());
    sol::table result = lua.create_table();
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        result.add(mro.raw_get<sol::object>(index));
    }
    return result;
}

}  // namespace ludork::standard::class_runtime::detail
