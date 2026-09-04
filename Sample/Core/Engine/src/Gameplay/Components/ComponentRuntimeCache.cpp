#include <Gameplay/Components/ComponentRuntimeCache.hpp>

#include <Runtime/Detail/RuntimeServices.hpp>
#include <Runtime/RuntimeSession.hpp>

#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>

#include <cstddef>
#include <stdexcept>

namespace {

constexpr const char* COMPONENT_CACHES_KEY = "Ludork.Engine.componentCaches";

std::size_t cacheIndex(ComponentRuntimeCacheKind kind) {
    return static_cast<std::size_t>(kind) + 1;
}

sol::table cacheFor(sol::state_view lua, ComponentRuntimeCacheKind kind) {
    sol::table caches =
        ludork::runtime::detail::registryTable(lua, COMPONENT_CACHES_KEY);
    const std::size_t index = cacheIndex(kind);
    const sol::object rawCache = caches.raw_get<sol::object>(index);
    if (rawCache.is<sol::table>()) {
        return rawCache.as<sol::table>();
    }
    sol::table cache = lua.create_table();
    sol::table metatable = lua.create_table();
    metatable.raw_set("__mode", "k");
    cache[sol::metatable_key] = metatable;
    caches.raw_set(index, cache);
    return cache;
}

}  // namespace

RuntimeValue ComponentRuntimeCache::get(ComponentRuntimeCacheKind kind,
                                        const RuntimeValue& key) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    const sol::object rawKey =
        ludork::runtime::binding::writeLuaValue(lua, key);
    const sol::object value = cacheFor(lua, kind).raw_get<sol::object>(rawKey);
    return value.valid()
               ? ludork::runtime::binding::readLuaValue<RuntimeValue>(value)
               : RuntimeValue();
}

void ComponentRuntimeCache::set(ComponentRuntimeCacheKind kind,
                                const RuntimeValue& key,
                                const RuntimeValue& value) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    cacheFor(lua, kind).raw_set(
        ludork::runtime::binding::writeLuaValue(lua, key),
        ludork::runtime::binding::writeLuaValue(lua, value));
}

ComponentRuntimeCache& componentRuntimeCache() {
    static ComponentRuntimeCache cache;
    return cache;
}
