#include <Runtime/RuntimeReference.hpp>
#include "BlueprintRuntimeInternal.hpp"

extern "C" {
#include <lua.h>
}

namespace ludork::runtime::blueprint_detail {

using namespace ludork::runtime::reference;

std::function<RuntimeValue(const RuntimeValue&)>& objectGraphResolver() {
    static std::function<RuntimeValue(const RuntimeValue&)> resolver;
    return resolver;
}

RuntimeValue objectGraph(const RuntimeValue& object) {
    const auto resolver = objectGraphResolver();
    return resolver ? resolver(object) : RuntimeValue();
}

void clearBlueprintRuntimeCaches(lua_State* state) noexcept {
    objectGraphResolver() = {};
    lua_pushnil(state);
    lua_setfield(state, LUA_REGISTRYINDEX, BLUEPRINT_IMPLEMENTATION_CACHE_KEY);
    lua_pushnil(state);
    lua_setfield(state, LUA_REGISTRYINDEX,
                 BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY);
    lua_pushnil(state);
    lua_setfield(state, LUA_REGISTRYINDEX,
                 BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY);
}

}  // namespace ludork::runtime::blueprint_detail
