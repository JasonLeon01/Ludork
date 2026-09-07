#include <ClassHotReload.hpp>

#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Native/NativeRuntime.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace ludork::standard::class_runtime {

namespace {

sol::table requireClass(lua_State* state, int index) {
    if (!isHotReloadClass(state, index)) {
        throw std::invalid_argument("Hot reload requires a finalized class");
    }
    return sol::stack::get<sol::table>(state, index);
}

sol::object mappedClass(const sol::table& candidateToLive,
                        const sol::object& candidate) {
    const sol::object mapped = candidateToLive.raw_get<sol::object>(candidate);
    return mapped.is<sol::table>() ? mapped : candidate;
}

void validateHierarchy(const sol::table& previous, const sol::table& candidate,
                       const sol::table& candidateToLive, const char* field) {
    const sol::object oldValue = previous.raw_get<sol::object>(field);
    const sol::object newValue = candidate.raw_get<sol::object>(field);
    if (!oldValue.is<sol::table>() || !newValue.is<sol::table>()) {
        throw std::invalid_argument(
            "Hot reload requires finalized class metadata");
    }
    const sol::table oldTypes = oldValue.as<sol::table>();
    const sol::table newTypes = newValue.as<sol::table>();
    if (oldTypes.size() != newTypes.size()) {
        throw std::runtime_error("Class inheritance changed; restart the game");
    }
    for (std::size_t index = 1; index <= oldTypes.size(); ++index) {
        const sol::object oldType = oldTypes.raw_get<sol::object>(index);
        const sol::object newType = newTypes.raw_get<sol::object>(index);
        if (detail::objectsRawEqual(oldType, previous) &&
            detail::objectsRawEqual(newType, candidate)) {
            continue;
        }
        if (!detail::objectsRawEqual(oldType,
                                     mappedClass(candidateToLive, newType))) {
            throw std::runtime_error(
                "Class inheritance changed; restart the game");
        }
    }
}

sol::table candidateClass(const sol::table& candidateToLive,
                          const sol::table& live) {
    for (const auto& entry : candidateToLive) {
        if (entry.first.is<sol::table>() && entry.second.is<sol::table>() &&
            detail::objectsRawEqual(entry.second, live) &&
            detail::isClass(entry.first.as<sol::table>())) {
            return entry.first.as<sol::table>();
        }
    }
    return live;
}

bool mixinHasCallback(sol::state_view lua, const sol::table& previous,
                      const sol::table& candidate,
                      const sol::table& candidateToLive,
                      const sol::object& key) {
    const sol::object own = candidate.raw_get<sol::object>(key);
    if (own.valid() && own.get_type() != sol::type::lua_nil) {
        return own.is<sol::function>();
    }
    const sol::table mro = detail::getMro(lua, previous);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>() ||
            !detail::isClass(rawType.as<sol::table>())) {
            continue;
        }
        const sol::table type =
            candidateClass(candidateToLive, rawType.as<sol::table>());
        const sol::object member = type.raw_get<sol::object>(key);
        if (member.valid() && member.get_type() != sol::type::lua_nil) {
            return member.is<sol::function>();
        }
    }
    return false;
}

void validateCallbacks(sol::state_view lua, const sol::table& previous,
                       const sol::table& candidate,
                       const sol::table& candidateToLive) {
    std::unordered_set<std::string> names;
    const sol::table mro = detail::getMro(lua, previous);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawType = mro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>() ||
            !detail::isNativeType(lua, rawType.as<sol::table>())) {
            continue;
        }
        const sol::object callbacks =
            detail::rawMember(lua, rawType.as<sol::table>(),
                              sol::make_object(lua, "__classCallbacks"));
        if (!callbacks.is<sol::table>()) {
            continue;
        }
        for (const auto& entry : callbacks.as<sol::table>()) {
            if (entry.second.is<std::string>()) {
                names.insert(entry.second.as<std::string>());
            }
        }
    }
    for (const std::string& name : names) {
        const sol::object key = sol::make_object(lua, name);
        const bool oldCallback =
            detail::findScriptMember(lua, previous, key).is<sol::function>();
        const bool newCallback =
            detail::isClass(candidate)
                ? detail::findScriptMember(lua, candidate, key)
                      .is<sol::function>()
                : mixinHasCallback(lua, previous, candidate, candidateToLive,
                                   key);
        if (oldCallback != newCallback) {
            throw std::runtime_error(
                "Native callback '" + name +
                "' changed availability; restart the game");
        }
    }
}

bool isBusinessMethod(const sol::object& key, const sol::object& value) {
    if (!key.is<std::string>() || !value.is<sol::function>()) {
        return false;
    }
    const std::string name = key.as<std::string>();
    return !name.starts_with("__") && name != "new";
}

}  // namespace

bool isHotReloadClass(lua_State* state, int index) {
    return lua_istable(state, index) != 0 &&
           detail::isClass(sol::stack::get<sol::table>(state, index));
}

bool isHotReloadProtectedTable(lua_State* state, int index) {
    if (lua_istable(state, index) == 0) {
        return false;
    }
    const int tableIndex = lua_absindex(state, index);
    lua_pushstring(state, detail::METHOD_OWNERS_KEY);
    lua_rawget(state, LUA_REGISTRYINDEX);
    const bool protectedTable = lua_rawequal(state, tableIndex, -1) != 0;
    lua_pop(state, 1);
    return protectedTable;
}

void validateHotReloadClass(lua_State* state, int oldIndex, int newIndex,
                            int candidateToLiveTableIndex) {
    const sol::table previous = requireClass(state, oldIndex);
    if (lua_istable(state, newIndex) == 0 ||
        lua_istable(state, candidateToLiveTableIndex) == 0) {
        throw std::invalid_argument(
            "Hot reload requires candidate and identity tables");
    }
    const sol::table candidate = sol::stack::get<sol::table>(state, newIndex);
    const sol::table candidateToLive =
        sol::stack::get<sol::table>(state, candidateToLiveTableIndex);
    if (detail::isClass(candidate)) {
        validateHierarchy(previous, candidate, candidateToLive, "__bases");
        validateHierarchy(previous, candidate, candidateToLive, "__mro");
    } else if (!detail::rawBool(previous, "_GENERATED_CLASS")) {
        throw std::runtime_error("Class export changed kind; restart the game");
    }
    validateCallbacks(sol::state_view(state), previous, candidate,
                      candidateToLive);
}

void commitHotReloadClass(lua_State* state, int oldIndex, int newIndex) {
    sol::table previous = requireClass(state, oldIndex);
    if (lua_istable(state, newIndex) == 0) {
        throw std::invalid_argument("Hot reload requires a candidate table");
    }
    sol::state_view lua(state);
    for (const auto& entry : previous) {
        if (isBusinessMethod(entry.first, entry.second)) {
            detail::registerMethodOwner(lua, previous, entry.second);
        }
    }
    previous.raw_set("_hasImplementationOwner", sol::lua_nil);
    detail::invalidateClassLookup(lua, previous);
}

}  // namespace ludork::standard::class_runtime
