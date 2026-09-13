#include <Components/ComponentRuntimeCache.hpp>
#include <Runtime/RuntimeReference.hpp>

extern "C" {
#include <lua.h>
}

namespace {
using namespace ludork::runtime::reference;
constexpr const char* COMPONENT_CACHES_KEY = "Ludork.Runtime.componentCaches";

RuntimeValue cacheFor(ComponentRuntimeCache::ComponentRuntimeCacheKind kind) {
    const RuntimeHandle caches = registryTable(COMPONENT_CACHES_KEY);
    const std::size_t index = static_cast<std::size_t>(kind) + 1;
    RuntimeValue cache = rawGet(caches, index);
    if (!isTable(cache)) {
        cache = table(WeakMode::Keys);
        rawSet(caches, index, cache);
    }
    return cache;
}
}  // namespace

void ComponentRuntimeCache::clear(lua_State* state) const noexcept {
    lua_pushnil(state);
    lua_setfield(state, LUA_REGISTRYINDEX, COMPONENT_CACHES_KEY);
}

RuntimeValue ComponentRuntimeCache::get(
    ComponentRuntimeCache::ComponentRuntimeCacheKind kind,
    const RuntimeValue& key) const {
    return ludork::runtime::reference::snapshot(getReference(kind, key));
}

RuntimeValue ComponentRuntimeCache::getReference(
    ComponentRuntimeCache::ComponentRuntimeCacheKind kind,
    const RuntimeValue& key) const {
    return ludork::runtime::reference::rawGet(
        ludork::runtime::reference::intern(cacheFor(kind)), key);
}

void ComponentRuntimeCache::set(
    ComponentRuntimeCache::ComponentRuntimeCacheKind kind,
    const RuntimeValue& key, const RuntimeValue& value) const {
    ludork::runtime::reference::rawSet(
        ludork::runtime::reference::intern(cacheFor(kind)), key, value);
}
ComponentRuntimeCache& componentRuntimeCache() {
    static ComponentRuntimeCache cache;
    return cache;
}
