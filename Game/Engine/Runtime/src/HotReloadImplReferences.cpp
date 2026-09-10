#include "HotReloadImpl.hpp"

#include <ClassHotReload.hpp>

namespace ludork::runtime {

int HotReloadImpl::replacement(int reference) const {
    if (!isLuaFunction(reference)) {
        return reference;
    }
    const auto found = functionsByLive_.find(pointer(reference));
    return found == functionsByLive_.end() ? reference : found->second;
}

void HotReloadImpl::visit(int reference) {
    const int type = getType(reference);
    if (type != LUA_TTABLE && type != LUA_TFUNCTION && type != LUA_TUSERDATA) {
        return;
    }
    const void* identity = pointer(reference);
    if (identity == referencesPointer_ || excluded_.contains(identity) ||
        !visited_.insert(identity).second) {
        return;
    }
    push(reference);
    const bool protectedTable =
        ludork::standard::class_runtime::isHotReloadProtectedTable(state_, -1);
    lua_pop(state_, 1);
    if (protectedTable) {
        return;
    }
    if (type == LUA_TTABLE) {
        for (const auto& [key, value] : entries(reference)) {
            const int newKey = replacement(key);
            const int newValue = replacement(value);
            if (newKey != key) {
                push(reference);
                push(newKey);
                lua_rawget(state_, -2);
                const bool collision = !lua_isnil(state_, -1);
                lua_pop(state_, 2);
                if (collision) {
                    fail("Lua references",
                         "replacing a function table key would collide");
                }
                writes_.push_back({Write::Kind::Table, reference, key, 0, 0});
            }
            if (newKey != key || newValue != value) {
                writes_.push_back(
                    {Write::Kind::Table, reference, newKey, newValue, 0});
            }
            pending_.push_back(key);
            pending_.push_back(value);
        }
    }
    if (type == LUA_TFUNCTION) {
        push(reference);
        const int function = lua_gettop(state_);
        for (int index = 1;; ++index) {
            if (lua_getupvalue(state_, function, index) == nullptr) {
                break;
            }
            const int value = keep(-1);
            lua_pop(state_, 1);
            const int newValue = replacement(value);
            if (newValue != value) {
                writes_.push_back(
                    {Write::Kind::Upvalue, reference, 0, newValue, index});
            }
            pending_.push_back(value);
        }
        lua_pop(state_, 1);
    }
    if (type == LUA_TUSERDATA) {
        push(reference);
        const int userdata = lua_gettop(state_);
        for (int index = 1;; ++index) {
            const int valueType = lua_getiuservalue(state_, userdata, index);
            if (valueType == LUA_TNONE) {
                lua_pop(state_, 1);
                break;
            }
            const int value = keep(-1);
            lua_pop(state_, 1);
            const int newValue = replacement(value);
            if (newValue != value) {
                writes_.push_back(
                    {Write::Kind::Uservalue, reference, 0, newValue, index});
            }
            pending_.push_back(value);
        }
        lua_pop(state_, 1);
    }
    push(reference);
    if (lua_getmetatable(state_, -1)) {
        pending_.push_back(keep(-1));
        lua_pop(state_, 1);
    }
    lua_pop(state_, 1);
}

void HotReloadImpl::prepareReferences() {
    excluded_.insert(pointer(environment_));
    excluded_.insert(pointer(package_));
    excluded_.insert(pointer(loaded_));
    excluded_.insert(pointer(candidateToLive_));
    for (const auto& [path, chunk] : chunks_) {
        static_cast<void>(path);
        excluded_.insert(pointer(chunk));
    }
    for (const auto& [name, candidate] : modules_) {
        const auto old = liveModules_.find(name);
        if (old == liveModules_.end() || !equal(old->second, candidate)) {
            excluded_.insert(pointer(candidate));
        }
    }
    lua_pushvalue(state_, LUA_REGISTRYINDEX);
    pending_.push_back(keep(-1));
    lua_pop(state_, 1);
    pending_.push_back(globals_);
    for (std::size_t index = 0; index < pending_.size(); ++index) {
        visit(pending_[index]);
    }
}

void HotReloadImpl::commit() {
    for (const Upvalue& upvalue : upvalues_) {
        push(upvalue.candidateFunction);
        push(upvalue.liveFunction);
        lua_upvaluejoin(state_, -2, upvalue.candidateIndex, -1,
                        upvalue.liveIndex);
        lua_pop(state_, 2);
    }
    for (const Write& write : writes_) {
        push(write.owner);
        if (write.kind == Write::Kind::Table) {
            push(write.key);
            push(write.value);
            lua_rawset(state_, -3);
        } else {
            push(write.value);
            if (write.kind == Write::Kind::Upvalue) {
                lua_setupvalue(state_, -2, write.index);
            } else {
                lua_setiuservalue(state_, -2, write.index);
            }
        }
        lua_pop(state_, 1);
    }
    for (const Pair& pair : classes_) {
        push(pair.live);
        push(pair.candidate);
        ludork::standard::class_runtime::commitHotReloadClass(state_, -2, -1);
        lua_pop(state_, 2);
    }
    for (const Pair& pair : mixinClasses_) {
        push(pair.live);
        push(pair.candidate);
        ludork::standard::class_runtime::commitHotReloadClass(state_, -2, -1);
        lua_pop(state_, 2);
    }
}

}  // namespace ludork::runtime
