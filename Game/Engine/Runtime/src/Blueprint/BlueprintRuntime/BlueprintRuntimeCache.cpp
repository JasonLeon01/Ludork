#include <Runtime/RuntimeReference.hpp>
#include "BlueprintRuntimeInternal.hpp"

extern "C" {
#include <lua.h>
}

namespace ludork::runtime::blueprint_detail {

using namespace ludork::runtime::reference;

ObjectGraphResolver& objectGraphResolver() {
    static ObjectGraphResolver resolver;
    return resolver;
}

std::shared_ptr<Graph> objectGraph(const RuntimeValue& object) {
    const auto& resolver = objectGraphResolver();
    return resolver ? resolver(ludork::runtime::reference::object(object))
                    : nullptr;
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
