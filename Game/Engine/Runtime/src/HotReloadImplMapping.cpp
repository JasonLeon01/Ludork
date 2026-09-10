#include "HotReloadImpl.hpp"
#include "ScriptModuleShape.hpp"

#include <ClassHotReload.hpp>

#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ludork::runtime {

void HotReloadImpl::pairValue(int live, int candidate,
                              const std::string& path) {
    if (equal(live, candidate)) {
        return;
    }
    const int liveType = getType(live);
    const int candidateType = getType(candidate);
    if ((liveType == LUA_TFUNCTION) != (candidateType == LUA_TFUNCTION)) {
        fail(path, "function/field kind changed");
    }
    if (liveType == LUA_TFUNCTION) {
        if (!isLuaFunction(live) || !isLuaFunction(candidate)) {
            fail(path, "native function identity changed");
        }
        const auto [candidateOwner, candidateAdded] =
            objects_.emplace(pointer(candidate), live);
        if (!candidateAdded && !equal(candidateOwner->second, live)) {
            fail(path, "distinct existing functions were merged");
        }
        const void* oldPointer = pointer(live);
        const auto previous = functionsByLive_.find(oldPointer);
        if (previous != functionsByLive_.end()) {
            if (!equal(previous->second, candidate)) {
                fail(path,
                     "one existing function has multiple candidate "
                     "implementations");
            }
            return;
        }
        functionsByLive_.emplace(oldPointer, candidate);
        functions_.push_back({live, candidate, path});
        excluded_.insert(pointer(candidate));
        return;
    }
    if (candidateType == LUA_TTABLE && liveType == LUA_TTABLE) {
        const void* newPointer = pointer(candidate);
        const auto previous = objects_.find(newPointer);
        if (previous != objects_.end()) {
            if (!equal(previous->second, live)) {
                fail(path, "shared definition identity changed");
            }
            return;
        }
        if (isClass(live) != isClass(candidate)) {
            fail(path, "class definition kind changed");
        }
        objects_.emplace(newPointer, live);
        push(candidateToLive_);
        push(candidate);
        push(live);
        lua_rawset(state_, -3);
        lua_pop(state_, 1);
        tables_.push_back({live, candidate, path});
        if (isClass(live)) {
            classes_.push_back({live, candidate, path});
        }
        excluded_.insert(newPointer);
    }
}

void HotReloadImpl::pairTable(const Pair& pair) {
    const bool classTable = isClass(pair.live);
    for (const auto& [key, candidate] : entries(pair.candidate)) {
        if (classTable && isClassInternal(key)) {
            continue;
        }
        push(pair.live);
        push(key);
        lua_rawget(state_, -2);
        const int live = keep(-1);
        lua_pop(state_, 2);
        const std::string path = pair.path + "." + stringValue(key);
        if (getType(live) == LUA_TNIL && getType(candidate) == LUA_TFUNCTION) {
            fail(path, "function was added");
        }
        pairValue(live, candidate, path);
    }
}

void HotReloadImpl::pairFunction(const Pair& pair) {
    std::map<std::string, std::pair<int, int>> oldUpvalues;
    int oldEnvironment = 0;
    push(pair.live);
    const int oldFunction = lua_gettop(state_);
    for (int index = 1;; ++index) {
        const char* name = lua_getupvalue(state_, oldFunction, index);
        if (name == nullptr) {
            break;
        }
        if (std::string_view(name) == "_ENV") {
            oldEnvironment = index;
            lua_pop(state_, 1);
            continue;
        }
        if (*name == '\0' ||
            !oldUpvalues.emplace(name, std::make_pair(index, keep(-1)))
                 .second) {
            fail(pair.path, "upvalue names are missing or ambiguous");
        }
        lua_pop(state_, 1);
    }
    lua_pop(state_, 1);
    push(pair.candidate);
    const int newFunction = lua_gettop(state_);
    std::size_t count = 0;
    for (int index = 1;; ++index) {
        const char* rawName = lua_getupvalue(state_, newFunction, index);
        if (rawName == nullptr) {
            break;
        }
        const std::string name(rawName);
        const int candidate = keep(-1);
        lua_pop(state_, 1);
        if (name == "_ENV") {
            environments_.push_back({pair.live, oldEnvironment, pair.candidate,
                                     index, candidate, pair.path});
            continue;
        }
        ++count;
        const auto old = oldUpvalues.find(name);
        if (old == oldUpvalues.end()) {
            fail(pair.path, "upvalue '" + name + "' was added");
        }
        const auto [oldIndex, live] = old->second;
        pairValue(live, candidate, pair.path + "::<" + name + ">");
        push(pair.live);
        const void* oldCell = lua_upvalueid(state_, -1, oldIndex);
        lua_pop(state_, 1);
        const void* newCell = lua_upvalueid(state_, newFunction, index);
        const auto [oldEntry, oldAdded] = liveCells_.emplace(oldCell, newCell);
        const auto [newEntry, newAdded] =
            candidateCells_.emplace(newCell, oldCell);
        if ((!oldAdded && oldEntry->second != newCell) ||
            (!newAdded && newEntry->second != oldCell)) {
            fail(pair.path, "shared upvalue topology changed");
        }
        upvalues_.push_back({pair.live, pair.candidate, oldIndex, index});
    }
    lua_pop(state_, 1);
    if (count != oldUpvalues.size()) {
        fail(pair.path, "upvalue was removed");
    }
}

void HotReloadImpl::prepareEnvironments() {
    std::unordered_map<const void*, const Environment*> donors;
    std::unordered_set<const void*> ambiguous;
    const auto cell = [&](int function, int index) {
        push(function);
        const void* result = lua_upvalueid(state_, -1, index);
        lua_pop(state_, 1);
        return result;
    };
    for (const Environment& environment : environments_) {
        if (environment.liveIndex == 0) {
            continue;
        }
        const void* candidateCell =
            cell(environment.candidateFunction, environment.candidateIndex);
        const auto [found, added] = donors.emplace(candidateCell, &environment);
        if (!added &&
            cell(found->second->liveFunction, found->second->liveIndex) !=
                cell(environment.liveFunction, environment.liveIndex)) {
            ambiguous.insert(candidateCell);
        }
    }
    for (const Environment& environment : environments_) {
        if (environment.liveIndex != 0) {
            upvalues_.push_back(
                {environment.liveFunction, environment.candidateFunction,
                 environment.liveIndex, environment.candidateIndex});
            continue;
        }
        const void* candidateCell =
            cell(environment.candidateFunction, environment.candidateIndex);
        if (ambiguous.contains(candidateCell)) {
            fail(environment.path,
                 "new _ENV has multiple existing environment cells");
        }
        const auto donor = donors.find(candidateCell);
        if (donor != donors.end()) {
            upvalues_.push_back(
                {donor->second->liveFunction, environment.candidateFunction,
                 donor->second->liveIndex, environment.candidateIndex});
            continue;
        }
        int liveEnvironment = globals_;
        if (!equal(environment.candidateValue, environment_)) {
            const auto mapped =
                objects_.find(pointer(environment.candidateValue));
            if (mapped == objects_.end()) {
                fail(environment.path,
                     "new custom _ENV has no matching live environment");
            }
            liveEnvironment = mapped->second;
        }
        writes_.push_back({Write::Kind::Upvalue, environment.candidateFunction,
                           0, liveEnvironment, environment.candidateIndex});
    }
}

void HotReloadImpl::preparePairs() {
    for (const auto& [name, live] : liveModules_) {
        static_cast<void>(live);
        if (scriptStore().ownsModule(state_, name) && name != "Entry" &&
            !name.ends_with("_meta")) {
            push(modules_.at(name));
            validateScriptModuleShape(state_, name, -1);
            lua_pop(state_, 1);
        }
    }
    for (const Pair& root : roots_) {
        if (getType(root.live) != getType(root.candidate)) {
            fail(root.path, "module return type changed");
        }
        pairValue(root.live, root.candidate, root.path);
    }
    std::size_t tableIndex = 0;
    std::size_t functionIndex = 0;
    while (tableIndex < tables_.size() || functionIndex < functions_.size()) {
        if (tableIndex < tables_.size()) {
            const Pair pair = tables_[tableIndex++];
            pairTable(pair);
        } else {
            const Pair pair = functions_[functionIndex++];
            pairFunction(pair);
        }
    }
    prepareEnvironments();
    for (const Pair& pair : classes_) {
        push(pair.live);
        push(pair.candidate);
        push(candidateToLive_);
        ludork::standard::class_runtime::validateHotReloadClass(state_, -3, -2,
                                                                -1);
        lua_pop(state_, 3);
    }
    for (const Pair& pair : mixinClasses_) {
        push(pair.live);
        push(pair.candidate);
        push(candidateToLive_);
        ludork::standard::class_runtime::validateHotReloadClass(state_, -3, -2,
                                                                -1);
        lua_pop(state_, 3);
    }
}

}  // namespace ludork::runtime
