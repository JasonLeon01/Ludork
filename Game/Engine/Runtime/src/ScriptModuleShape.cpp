#include "ScriptModuleShape.hpp"
#include <ClassRuntimeProtocol.hpp>

#include <ClassHotReload.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace ludork::runtime {
namespace {

constexpr const char* MODULE_SHAPES = "Ludork.ScriptStore.moduleShapes";

bool internalKey(lua_State* state, int key, bool classTable) {
    if (!classTable || lua_type(state, key) != LUA_TSTRING) {
        return false;
    }
    std::size_t length = 0;
    const char* text = lua_tolstring(state, key, &length);
    const std::string_view name(text, length);
    return name == "new" || name == "_hasImplementationOwner" ||
           (name.starts_with("__") &&
            name != ludork::standard::class_runtime::protocol::
                        CLASS_GETTERS_FIELD &&
            name !=
                ludork::standard::class_runtime::protocol::CLASS_SETTERS_FIELD);
}

void pushShape(lua_State* state, int value, int seen) {
    value = lua_absindex(state, value);
    if (lua_checkstack(state, 10) == 0) {
        throw std::runtime_error(
            "Module definition shape exceeds the Lua stack");
    }
    const int kind = lua_type(state, value);
    if (kind == LUA_TTABLE) {
        lua_rawgetp(state, seen, lua_topointer(state, value));
        if (!lua_isnil(state, -1)) {
            return;
        }
        lua_pop(state, 1);
    }
    lua_newtable(state);
    const int shape = lua_gettop(state);
    lua_pushinteger(state, kind);
    lua_setfield(state, shape, "kind");
    if (kind != LUA_TTABLE) {
        return;
    }
    lua_pushvalue(state, shape);
    lua_rawsetp(state, seen, lua_topointer(state, value));
    if (standard::class_runtime::isHotReloadNativeType(state, value)) {
        lua_pushvalue(state, value);
        lua_setfield(state, shape, "nativeType");
        return;
    }
    const bool classTable =
        standard::class_runtime::isHotReloadClass(state, value);
    lua_pushboolean(state, classTable);
    lua_setfield(state, shape, "class");
    lua_newtable(state);
    const int members = lua_gettop(state);
    lua_pushnil(state);
    while (lua_next(state, value) != 0) {
        const int keyKind = lua_type(state, -2);
        if (!internalKey(state, -2, classTable)) {
            if (keyKind == LUA_TSTRING || keyKind == LUA_TNUMBER ||
                keyKind == LUA_TBOOLEAN) {
                lua_pushvalue(state, -2);
                pushShape(state, -2, seen);
                lua_rawset(state, members);
            } else {
                lua_pushboolean(state, true);
                lua_setfield(state, shape, "complexKeys");
            }
        }
        lua_pop(state, 1);
    }
    lua_setfield(state, shape, "members");
}

lua_Integer integerField(lua_State* state, int table, const char* field) {
    lua_getfield(state, table, field);
    const lua_Integer result = lua_tointeger(state, -1);
    lua_pop(state, 1);
    return result;
}

bool booleanField(lua_State* state, int table, const char* field) {
    lua_getfield(state, table, field);
    const bool result = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    return result;
}

std::string fieldPath(lua_State* state, int key, const std::string& path) {
    if (lua_type(state, key) == LUA_TBOOLEAN) {
        return path + (lua_toboolean(state, key) ? "[true]" : "[false]");
    }
    lua_pushvalue(state, key);
    std::size_t length = 0;
    const char* text = lua_tolstring(state, -1, &length);
    const std::string result = path + "." + std::string(text, length);
    lua_pop(state, 1);
    return result;
}

void compareShapes(lua_State* state, int oldShape, int newShape,
                   const std::string& path,
                   std::unordered_map<const void*, const void*>& oldToNew,
                   std::unordered_map<const void*, const void*>& newToOld) {
    const auto fail = [&](const char* reason) {
        throw std::runtime_error(path + ": " + reason + "; restart required");
    };
    if (lua_checkstack(state, 10) == 0) {
        fail("definition shape exceeds the Lua stack");
    }
    if (!lua_istable(state, oldShape) || !lua_istable(state, newShape)) {
        fail("definition field was added or removed");
    }
    oldShape = lua_absindex(state, oldShape);
    newShape = lua_absindex(state, newShape);
    const int kind = integerField(state, oldShape, "kind");
    if (kind != integerField(state, newShape, "kind") ||
        booleanField(state, oldShape, "class") !=
            booleanField(state, newShape, "class")) {
        fail("definition field type changed");
    }
    if (kind != LUA_TTABLE) {
        return;
    }
    lua_getfield(state, oldShape, "nativeType");
    lua_getfield(state, newShape, "nativeType");
    const bool nativeType = !lua_isnil(state, -2) || !lua_isnil(state, -1);
    const bool sameNativeType = lua_rawequal(state, -2, -1) != 0;
    lua_pop(state, 2);
    if (nativeType) {
        if (!sameNativeType) {
            fail("native type reference changed");
        }
        return;
    }
    const void* oldId = lua_topointer(state, oldShape);
    const void* newId = lua_topointer(state, newShape);
    const auto [forward, added] = oldToNew.emplace(oldId, newId);
    const auto [reverse, reverseAdded] = newToOld.emplace(newId, oldId);
    if (forward->second != newId || reverse->second != oldId) {
        fail("shared definition structure changed");
    }
    if (!added && !reverseAdded) {
        return;
    }
    if (booleanField(state, oldShape, "complexKeys") ||
        booleanField(state, newShape, "complexKeys")) {
        fail("object keys cannot be matched for hot reload");
    }
    lua_getfield(state, oldShape, "members");
    const int oldMembers = lua_gettop(state);
    lua_getfield(state, newShape, "members");
    const int newMembers = lua_gettop(state);
    lua_pushnil(state);
    while (lua_next(state, oldMembers) != 0) {
        lua_pushvalue(state, -2);
        lua_rawget(state, newMembers);
        compareShapes(state, -2, -1, fieldPath(state, -3, path), oldToNew,
                      newToOld);
        lua_pop(state, 2);
    }
    lua_pushnil(state);
    while (lua_next(state, newMembers) != 0) {
        lua_pushvalue(state, -2);
        lua_rawget(state, oldMembers);
        if (lua_isnil(state, -1)) {
            throw std::runtime_error(
                fieldPath(state, -3, path) +
                ": definition field was added; restart required");
        }
        lua_pop(state, 2);
    }
    lua_pop(state, 2);
}

}  // namespace

void captureScriptModuleShape(lua_State* state, const std::string& name,
                              int definition) {
    const int top = lua_gettop(state);
    definition = lua_absindex(state, definition);
    try {
        lua_newtable(state);
        pushShape(state, definition, lua_gettop(state));
        luaL_getsubtable(state, LUA_REGISTRYINDEX, MODULE_SHAPES);
        lua_pushlstring(state, name.data(), name.size());
        lua_pushvalue(state, -3);
        lua_rawset(state, -3);
    } catch (...) {
        lua_settop(state, top);
        throw;
    }
    lua_settop(state, top);
}

void validateScriptModuleShape(lua_State* state, const std::string& name,
                               int candidate) {
    const int top = lua_gettop(state);
    candidate = lua_absindex(state, candidate);
    try {
        luaL_getsubtable(state, LUA_REGISTRYINDEX, MODULE_SHAPES);
        lua_getfield(state, -1, name.c_str());
        const int previous = lua_gettop(state);
        if (lua_isnil(state, previous)) {
            throw std::runtime_error(
                name +
                ": initial definition shape is unavailable; restart required");
        }
        lua_newtable(state);
        pushShape(state, candidate, lua_gettop(state));
        std::unordered_map<const void*, const void*> oldToNew;
        std::unordered_map<const void*, const void*> newToOld;
        compareShapes(state, previous, -1, name, oldToNew, newToOld);
    } catch (...) {
        lua_settop(state, top);
        throw;
    }
    lua_settop(state, top);
}

}  // namespace ludork::runtime
