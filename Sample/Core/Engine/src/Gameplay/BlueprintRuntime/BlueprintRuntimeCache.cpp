#include "BlueprintRuntimeInternal.hpp"

namespace ludork::engine::runtime_detail {

void clearBlueprintRuntimeCaches(sol::state_view lua) {
    lua.registry().raw_set(BLUEPRINT_IMPLEMENTATION_CACHE_KEY, sol::lua_nil);
    lua.registry().raw_set(BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY, sol::lua_nil);
    lua.registry().raw_set(BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY,
                           sol::lua_nil);
}

}  // namespace ludork::engine::runtime_detail
