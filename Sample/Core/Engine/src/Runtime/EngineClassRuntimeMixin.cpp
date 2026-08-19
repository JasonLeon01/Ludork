#include "EngineClassRuntimeInternal.hpp"
#include "RuntimeSubsystemServices.hpp"

#include <ClassServices.hpp>
#include <Gameplay/Components/ComponentRuntime.hpp>
#include <Runtime/RuntimeValue.hpp>
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

std::string scriptMixinModuleName(const std::string& scriptPath) {
    std::string module =
        "Mixins." + scriptPath.substr(0, scriptPath.size() - 4);
    std::replace(module.begin(), module.end(), '/', '.');
    return module;
}

std::string findLuaModuleFile(sol::state_view lua,
                              const std::string& moduleName) {
    const sol::object rawPackage =
        lua.globals().raw_get<sol::object>("package");
    if (!rawPackage.is<sol::table>()) {
        throw std::runtime_error("Lua package table is unavailable");
    }
    const sol::table package = rawPackage.as<sol::table>();
    const sol::object rawSearch = package.raw_get<sol::object>("searchpath");
    const sol::object rawPath = package.raw_get<sol::object>("path");
    if (!rawSearch.is<sol::protected_function>() ||
        !rawPath.is<std::string>()) {
        throw std::runtime_error("Lua package.searchpath is unavailable");
    }
    sol::protected_function search = rawSearch.as<sol::protected_function>();
    sol::protected_function_result result = search(moduleName, rawPath);
    if (result.valid() && result.return_count() > 0) {
        const sol::object found = result.get<sol::object>();
        if (found.is<std::string>()) {
            return found.as<std::string>();
        }
    }
    throw std::runtime_error("Mixin script was not found: " + moduleName);
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
    const std::string moduleName = scriptMixinModuleName(scriptPath);
    const std::string filePath = findLuaModuleFile(lua, moduleName);
    sol::load_result loaded = lua.load_file(filePath);
    if (!loaded.valid()) {
        const sol::error error = loaded;
        throw std::runtime_error("Failed to load Mixin " + scriptPath +
                                 " for " + classPath + ": " + error.what());
    }
    sol::protected_function chunk = loaded;
    sol::protected_function_result result = chunk();
    sol::object value;
    try {
        value = checkedResult(lua, result);
    } catch (const std::runtime_error& error) {
        throw std::runtime_error("Failed to execute Mixin " + scriptPath +
                                 " for " + classPath + ": " + error.what());
    }
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
