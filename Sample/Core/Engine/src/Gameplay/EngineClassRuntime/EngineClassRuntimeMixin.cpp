#include "EngineClassRuntimeInternal.hpp"
#include <Runtime/Detail/RuntimeServices.hpp>

#include <ClassServices.hpp>
#include <Gameplay/Components/ComponentRuntime.hpp>
#include <LuaError.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <Runtime/ScriptStore.hpp>
#include <RuntimeSession.hpp>
#include <Utils/DataValue.hpp>

#include <sol2/sol.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::engine::class_runtime_detail {

std::string normalizeScriptMixinPath(const std::string& value) {
    std::string path = value;
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.empty() || path.front() == '/' ||
        path.find(':') != std::string::npos || !path.ends_with(".lua") ||
        path.ends_with("_meta.lua")) {
        throw std::runtime_error(
            "scriptPath must be a relative .lua path under Scripts/Mixins");
    }
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = path.find('/', start);
        const std::string part = path.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (part.empty() || part == "." || part == "..") {
            throw std::runtime_error("scriptPath cannot leave Scripts/Mixins");
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return path;
}

bool tableHasMetatable(sol::state_view lua, const sol::table& table) {
    lua_State* state = lua.lua_state();
    table.push();
    const bool hasMetatable = lua_getmetatable(state, -1) != 0;
    lua_pop(state, hasMetatable ? 2 : 1);
    return hasMetatable;
}

bool isScriptMixinReservedName(const std::string& name) {
    return name.starts_with("__") || name == "init" || name == "new" ||
           name == "scriptMixin" || name == "scriptPath" ||
           name == "_GENERATED_CLASS" || name == "_graph" ||
           name == "_hasImplementationOwner";
}

sol::table loadScriptMixin(sol::state_view lua, const std::string& classPath,
                           const std::string& scriptPath) {
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    if (ludork::runtime::scriptStore().loadFile(
            state, "Scripts/Mixins/" + scriptPath) != LUA_OK) {
        const std::string error = ludork::standard::luaErrorMessage(state, -1);
        lua_settop(state, stackBase);
        throw std::runtime_error("Failed to load Mixin " + scriptPath +
                                 " for " + classPath + ": " + error);
    }
    if (ludork::standard::protectedLuaCall(state, 0, 1) != LUA_OK) {
        const std::string error = ludork::standard::luaErrorMessage(state, -1);
        lua_settop(state, stackBase);
        throw std::runtime_error("Failed to execute Mixin " + scriptPath +
                                 " for " + classPath + ": " + error);
    }
    sol::object value = sol::stack::get<sol::object>(state, -1);
    lua_settop(state, stackBase);
    if (!value.is<sol::table>()) {
        throw std::runtime_error("Mixin " + scriptPath + " for " + classPath +
                                 " must return a table");
    }
    sol::table mixin = value.as<sol::table>();
    if (tableHasMetatable(lua, mixin)) {
        throw std::runtime_error("Mixin " + scriptPath + " for " + classPath +
                                 " must return a table without a metatable");
    }
    for (const auto& entry : mixin) {
        if (!entry.first.is<std::string>()) {
            throw std::runtime_error("Mixin " + scriptPath + " for " +
                                     classPath +
                                     " must use string member names");
        }
        const std::string name = entry.first.as<std::string>();
        if (isScriptMixinReservedName(name)) {
            throw std::runtime_error("Mixin " + scriptPath + " for " +
                                     classPath + " uses reserved member '" +
                                     name + "'");
        }
    }
    return mixin;
}

void mergeScriptMixin(sol::state_view lua, const sol::table& parentClass,
                      const sol::table& mixin, sol::table definition,
                      sol::table instanceAttrs, const std::string& classPath,
                      const std::string& scriptPath) {
    for (const auto& entry : mixin) {
        const std::string name = entry.first.as<std::string>();
        const sol::object inherited = parentClass.get<sol::object>(name);
        const bool valueIsFunction =
            entry.second.get_type() == sol::type::function;
        const bool inheritedExists =
            inherited.valid() && inherited.get_type() != sol::type::lua_nil;
        if (inheritedExists &&
            (inherited.get_type() == sol::type::function) != valueIsFunction) {
            throw std::runtime_error(
                "Mixin " + scriptPath + " for " + classPath +
                " changes the member kind of '" + name + "'");
        }
        if (valueIsFunction) {
            definition.raw_set(name, entry.second);
        } else {
            definition.raw_set(name, ludork::standard::class_runtime::deepCopy(
                                         lua, entry.second));
            instanceAttrs.raw_set(
                name,
                ludork::standard::class_runtime::deepCopy(lua, entry.second));
        }
    }
}

}  // namespace ludork::engine::class_runtime_detail
