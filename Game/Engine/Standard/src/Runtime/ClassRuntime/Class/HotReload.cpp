#include <ClassHotReload.hpp>

#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypeQueries.hpp"
#include "Native/NativeRuntime.hpp"

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace ludork::standard::class_runtime {

namespace {

lua_glue::Table requireClass(lua_State* state, int index) {
    if (!isHotReloadClass(state, index)) {
        throw std::invalid_argument("Hot reload requires a finalized class");
    }
    return lua_glue::Read<lua_glue::Table>(state, index);
}

lua_glue::Object mappedClass(const lua_glue::Table& candidateToLive,
                             const lua_glue::Object& candidate) {
    const lua_glue::Object mapped =
        candidateToLive.raw_get<lua_glue::Object>(candidate);
    return mapped.is<lua_glue::Table>() ? mapped : candidate;
}

void validateHierarchy(const lua_glue::Table& previous,
                       const lua_glue::Table& candidate,
                       const lua_glue::Table& candidateToLive,
                       const char* field) {
    const lua_glue::Object oldValue = previous.raw_get<lua_glue::Object>(field);
    const lua_glue::Object newValue =
        candidate.raw_get<lua_glue::Object>(field);
    if (!oldValue.is<lua_glue::Table>() || !newValue.is<lua_glue::Table>()) {
        throw std::invalid_argument(
            "Hot reload requires finalized class metadata");
    }
    const lua_glue::Table oldTypes = oldValue.as<lua_glue::Table>();
    const lua_glue::Table newTypes = newValue.as<lua_glue::Table>();
    if (oldTypes.size() != newTypes.size()) {
        throw std::runtime_error("Class inheritance changed; restart the game");
    }
    for (std::size_t index = 1; index <= oldTypes.size(); ++index) {
        const lua_glue::Object oldType =
            oldTypes.raw_get<lua_glue::Object>(index);
        const lua_glue::Object newType =
            newTypes.raw_get<lua_glue::Object>(index);
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

lua_glue::Table candidateClass(const lua_glue::Table& candidateToLive,
                               const lua_glue::Table& live) {
    for (const auto& entry : candidateToLive) {
        if (entry.first.is<lua_glue::Table>() &&
            entry.second.is<lua_glue::Table>() &&
            detail::objectsRawEqual(entry.second, live) &&
            detail::isClass(entry.first.as<lua_glue::Table>())) {
            return entry.first.as<lua_glue::Table>();
        }
    }
    return live;
}

bool mixinHasCallback(lua_glue::StateView lua, const lua_glue::Table& previous,
                      const lua_glue::Table& candidate,
                      const lua_glue::Table& candidateToLive,
                      const lua_glue::Object& key) {
    const lua_glue::Object own = candidate.raw_get<lua_glue::Object>(key);
    if (own.valid() && own.get_type() != lua_glue::Type::Nil) {
        return own.is<lua_glue::Function>();
    }
    const lua_glue::Table mro = detail::getMro(lua, previous);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const lua_glue::Object rawType = mro.raw_get<lua_glue::Object>(index);
        if (!rawType.is<lua_glue::Table>() ||
            !detail::isClass(rawType.as<lua_glue::Table>())) {
            continue;
        }
        const lua_glue::Table type =
            candidateClass(candidateToLive, rawType.as<lua_glue::Table>());
        const lua_glue::Object member = type.raw_get<lua_glue::Object>(key);
        if (member.valid() && member.get_type() != lua_glue::Type::Nil) {
            return member.is<lua_glue::Function>();
        }
    }
    return false;
}

void validateCallbacks(lua_glue::StateView lua, const lua_glue::Table& previous,
                       const lua_glue::Table& candidate,
                       const lua_glue::Table& candidateToLive) {
    std::unordered_set<std::string> names;
    const lua_glue::Table mro = detail::getMro(lua, previous);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const lua_glue::Object rawType = mro.raw_get<lua_glue::Object>(index);
        if (!rawType.is<lua_glue::Table>() ||
            !detail::isNativeType(lua, rawType.as<lua_glue::Table>())) {
            continue;
        }
        const lua_glue::Object callbacks = detail::rawMember(
            lua, rawType.as<lua_glue::Table>(),
            lua_glue::MakeObject(lua, detail::CLASS_CALLBACKS_FIELD));
        if (!callbacks.is<lua_glue::Table>()) {
            continue;
        }
        for (const auto& entry : callbacks.as<lua_glue::Table>()) {
            if (entry.second.is<std::string>()) {
                names.insert(entry.second.as<std::string>());
            }
        }
    }
    for (const std::string& name : names) {
        const lua_glue::Object key = lua_glue::MakeObject(lua, name);
        const bool oldCallback = detail::findScriptMember(lua, previous, key)
                                     .is<lua_glue::Function>();
        const bool newCallback =
            detail::isClass(candidate)
                ? detail::findScriptMember(lua, candidate, key)
                      .is<lua_glue::Function>()
                : mixinHasCallback(lua, previous, candidate, candidateToLive,
                                   key);
        if (oldCallback != newCallback) {
            throw std::runtime_error(
                "Native callback '" + name +
                "' changed availability; restart the game");
        }
    }
}

bool isBusinessMethod(const lua_glue::Object& key,
                      const lua_glue::Object& value) {
    if (!key.is<std::string>() || !value.is<lua_glue::Function>()) {
        return false;
    }
    const std::string name = key.as<std::string>();
    return !name.starts_with("__") && name != "new";
}

}  // namespace

bool isHotReloadClass(lua_State* state, int index) {
    return lua_istable(state, index) != 0 &&
           detail::isClass(lua_glue::Read<lua_glue::Table>(state, index));
}

bool isHotReloadNativeType(lua_State* state, int index) {
    return lua_istable(state, index) != 0 &&
           detail::isNativeType(lua_glue::StateView(state),
                                lua_glue::Read<lua_glue::Table>(state, index));
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
    const lua_glue::Table previous = requireClass(state, oldIndex);
    if (lua_istable(state, newIndex) == 0 ||
        lua_istable(state, candidateToLiveTableIndex) == 0) {
        throw std::invalid_argument(
            "Hot reload requires candidate and identity tables");
    }
    const lua_glue::Table candidate =
        lua_glue::Read<lua_glue::Table>(state, newIndex);
    const lua_glue::Table candidateToLive =
        lua_glue::Read<lua_glue::Table>(state, candidateToLiveTableIndex);
    if (detail::isClass(candidate)) {
        validateHierarchy(previous, candidate, candidateToLive,
                          detail::BASES_FIELD);
        validateHierarchy(previous, candidate, candidateToLive,
                          detail::MRO_FIELD);
    } else if (!detail::rawBool(previous, "_GENERATED_CLASS")) {
        throw std::runtime_error("Class export changed kind; restart the game");
    }
    validateCallbacks(lua_glue::StateView(state), previous, candidate,
                      candidateToLive);
}

void commitHotReloadClass(lua_State* state, int oldIndex, int newIndex) {
    lua_glue::Table previous = requireClass(state, oldIndex);
    if (lua_istable(state, newIndex) == 0) {
        throw std::invalid_argument("Hot reload requires a candidate table");
    }
    lua_glue::StateView lua(state);
    for (const auto& entry : previous) {
        if (isBusinessMethod(entry.first, entry.second)) {
            detail::registerMethodOwner(lua, previous, entry.second);
        }
    }
    previous.raw_set("_hasImplementationOwner", lua_glue::nil);
    detail::invalidateClassLookup(lua, previous);
}

}  // namespace ludork::standard::class_runtime
