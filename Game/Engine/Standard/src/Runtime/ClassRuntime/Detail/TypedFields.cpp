#include "Detail/TypedFields.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

namespace ludork::standard::class_runtime::detail {

namespace {

unsigned char explicitNilFieldsKey;

bool pushExplicitNilFields(lua_State* state, int targetIndex, bool create) {
    targetIndex = lua_absindex(state, targetIndex);
    lua_rawgetp(state, LUA_REGISTRYINDEX, &explicitNilFieldsKey);
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        if (!create) {
            return false;
        }
        lua_newtable(state);
        lua_newtable(state);
        lua_pushliteral(state, "k");
        lua_setfield(state, -2, "__mode");
        lua_setmetatable(state, -2);
        lua_pushvalue(state, -1);
        lua_rawsetp(state, LUA_REGISTRYINDEX, &explicitNilFieldsKey);
    }
    lua_pushvalue(state, targetIndex);
    lua_rawget(state, -2);
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        if (!create) {
            lua_pop(state, 1);
            return false;
        }
        lua_newtable(state);
        lua_pushvalue(state, targetIndex);
        lua_pushvalue(state, -2);
        lua_rawset(state, -4);
    }
    lua_remove(state, -2);
    return true;
}

bool hasRawValue(lua_State* state, int targetIndex, int keyIndex) {
    if (lua_istable(state, targetIndex)) {
        lua_pushvalue(state, keyIndex);
        lua_rawget(state, targetIndex);
        const bool present = !lua_isnil(state, -1);
        lua_pop(state, 1);
        return present;
    }
    if (lua_type(state, targetIndex) != LUA_TUSERDATA) {
        return false;
    }
    const bool hasFields =
        lua_getiuservalue(state, targetIndex, 1) == LUA_TTABLE;
    bool present = false;
    if (hasFields) {
        lua_pushvalue(state, keyIndex);
        lua_rawget(state, -2);
        present = !lua_isnil(state, -1);
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    return present;
}

}  // namespace

bool hasExplicitNilField(lua_State* state, int targetIndex, int keyIndex) {
    targetIndex = lua_absindex(state, targetIndex);
    keyIndex = lua_absindex(state, keyIndex);
    if (!pushExplicitNilFields(state, targetIndex, false)) {
        return false;
    }
    lua_pushvalue(state, keyIndex);
    lua_rawget(state, -2);
    bool present = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    if (present && hasRawValue(state, targetIndex, keyIndex)) {
        lua_pushvalue(state, keyIndex);
        lua_pushnil(state);
        lua_rawset(state, -3);
        present = false;
    }
    lua_pop(state, 1);
    return present;
}

void clearExplicitNilField(lua_State* state, int targetIndex, int keyIndex) {
    targetIndex = lua_absindex(state, targetIndex);
    keyIndex = lua_absindex(state, keyIndex);
    if (pushExplicitNilFields(state, targetIndex, false)) {
        lua_pushvalue(state, keyIndex);
        lua_pushnil(state);
        lua_rawset(state, -3);
        lua_pop(state, 1);
    }
}

bool hasExplicitNilField(sol::state_view lua, const sol::object& target,
                         const sol::object& key) {
    target.push();
    key.push();
    const bool present = hasExplicitNilField(lua.lua_state(), -2, -1);
    lua_pop(lua.lua_state(), 2);
    return present;
}

void clearExplicitNilField(sol::state_view lua, const sol::object& target,
                           const sol::object& key) {
    target.push();
    key.push();
    clearExplicitNilField(lua.lua_state(), -2, -1);
    lua_pop(lua.lua_state(), 2);
}

void markExplicitNilField(sol::state_view lua, const sol::object& target,
                          const sol::object& key) {
    target.push();
    pushExplicitNilFields(lua.lua_state(), -1, true);
    key.push();
    lua_pushboolean(lua.lua_state(), true);
    lua_rawset(lua.lua_state(), -3);
    lua_pop(lua.lua_state(), 2);
}

sol::table explicitNilFieldKeys(sol::state_view lua,
                                const sol::object& target) {
    sol::table result = lua.create_table();
    lua_State* state = lua.lua_state();
    target.push();
    if (pushExplicitNilFields(state, -1, false)) {
        const sol::table fields = sol::stack::get<sol::table>(state, -1);
        lua_pop(state, 1);
        for (const auto& entry : fields) {
            if (hasExplicitNilField(lua, target, entry.first)) {
                result.add(entry.first);
            }
        }
    }
    lua_pop(state, 1);
    return result;
}

void copyExplicitNilFields(sol::state_view lua, const sol::object& source,
                           const sol::object& target) {
    if (target.get_type() != sol::type::table &&
        target.get_type() != sol::type::userdata) {
        return;
    }
    for (const auto& entry : explicitNilFieldKeys(lua, source)) {
        markExplicitNilField(lua, target, entry.second);
    }
}

void clearExplicitNilFields(sol::state_view lua, const sol::object& target) {
    lua_State* state = lua.lua_state();
    lua_rawgetp(state, LUA_REGISTRYINDEX, &explicitNilFieldsKey);
    if (lua_istable(state, -1)) {
        target.push();
        lua_pushnil(state);
        lua_rawset(state, -3);
    }
    lua_pop(state, 1);
}

void clearExplicitNilFields(lua_State* state) {
    lua_pushnil(state);
    lua_rawsetp(state, LUA_REGISTRYINDEX, &explicitNilFieldsKey);
}

}  // namespace ludork::standard::class_runtime::detail
