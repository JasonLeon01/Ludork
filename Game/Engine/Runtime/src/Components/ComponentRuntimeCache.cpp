#include <Components/ComponentRuntimeCache.hpp>
#include "LuaServices/RuntimeBindingTraits.hpp"
#include "LuaServices/RuntimeReferenceConversion.hpp"
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}
#include <new>

namespace {
constexpr const char* COMPONENT_CACHES_KEY = "Ludork.Runtime.componentCaches";
constexpr const char* COMPONENT_CACHE_METATABLE =
    "Ludork.Runtime.componentDescriptor";

int destroyDescriptor(lua_State* state) {
    static_cast<ComponentRuntimeCache::Entry*>(lua_touserdata(state, 1))
        ->~Entry();
    return 0;
}

void pushCache(lua_State* state, ComponentRuntimeCache::Kind kind) {
    lua_getfield(state, LUA_REGISTRYINDEX, COMPONENT_CACHES_KEY);
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_pushvalue(state, -1);
        lua_setfield(state, LUA_REGISTRYINDEX, COMPONENT_CACHES_KEY);
    }
    const int index = static_cast<int>(kind) + 1;
    lua_rawgeti(state, -1, index);
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_newtable(state);
        lua_pushliteral(state, "k");
        lua_setfield(state, -2, "__mode");
        lua_setmetatable(state, -2);
        lua_pushvalue(state, -1);
        lua_rawseti(state, -3, index);
    }
}
}  // namespace

ComponentRuntimeCache::Lease::Lease(Kind kind, const RuntimeValue& key)
    : stackBase_(lua_gettop(scope_.state())) {
    lua_State* state = scope_.state();
    try {
        pushCache(state, kind);
        cacheIndex_ = lua_gettop(state);
        ludork::runtime::binding::writeLuaValue(lua_glue::StateView(state), key)
            .push(state);
        keyIndex_ = lua_gettop(state);
        lua_pushvalue(state, keyIndex_);
        lua_rawget(state, cacheIndex_);
        if (lua_isnil(state, -1)) {
            lua_pop(state, 1);
            entry_ = new (lua_newuserdatauv(state, sizeof(Entry), 1)) Entry();
            switch (kind) {
                case Kind::Types:
                    break;
                case Kind::FieldDefaults:
                    entry_->descriptor.emplace<FieldDefaults>();
                    break;
                case Kind::FieldMap:
                    entry_->descriptor.emplace<FieldMap>();
                    break;
                case Kind::InheritedDefaults:
                    entry_->descriptor.emplace<InheritedDefaults>();
                    break;
            }
            if (luaL_newmetatable(state, COMPONENT_CACHE_METATABLE)) {
                lua_pushcfunction(state, destroyDescriptor);
                lua_setfield(state, -2, "__gc");
            }
            lua_setmetatable(state, -2);
            lua_newtable(state);
            lua_setiuservalue(state, -2, 1);
        } else {
            entry_ = static_cast<Entry*>(
                luaL_checkudata(state, -1, COMPONENT_CACHE_METATABLE));
        }
        userdataIndex_ = lua_gettop(state);
    } catch (...) {
        lua_settop(state, stackBase_);
        throw;
    }
}
ComponentRuntimeCache::Lease::~Lease() {
    lua_settop(scope_.state(), stackBase_);
}
bool ComponentRuntimeCache::Lease::initialized() const noexcept {
    return entry_->initialized;
}
void ComponentRuntimeCache::Lease::complete() noexcept {
    lua_State* state = scope_.state();
    lua_pushvalue(state, keyIndex_);
    lua_pushvalue(state, userdataIndex_);
    lua_rawset(state, cacheIndex_);
    entry_->initialized = true;
}
int ComponentRuntimeCache::Lease::store(const RuntimeValue& value) {
    lua_State* state = scope_.state();
    const int slot = entry_->nextSlot++;
    const lua_glue::Object raw = ludork::runtime::binding::writeLuaValue(
        lua_glue::StateView(state), value);
    lua_getiuservalue(state, userdataIndex_, 1);
    raw.push(state);
    lua_rawseti(state, -2, slot);
    lua_pop(state, 1);
    return slot;
}
RuntimeValue ComponentRuntimeCache::Lease::value(int slot) const {
    lua_State* state = scope_.state();
    lua_getiuservalue(state, userdataIndex_, 1);
    lua_rawgeti(state, -1, slot);
    const auto popper = lua_glue::PopGuard(state, 2);
    return ludork::runtime::detail::readRuntimeReference(
        lua_glue::Read<lua_glue::Object>(state, -1));
}
void ComponentRuntimeCache::clear(lua_State* state) const noexcept {
    lua_pushnil(state);
    lua_setfield(state, LUA_REGISTRYINDEX, COMPONENT_CACHES_KEY);
}
ComponentRuntimeCache& componentRuntimeCache() {
    static ComponentRuntimeCache cache;
    return cache;
}
