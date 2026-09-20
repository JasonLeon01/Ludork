#include "Bindings.hpp"

#include "Runtime/ContainerRuntime.hpp"

#include <LuaGlue/LuaGlue.hpp>

namespace ludork::standard::binding {

void registerContainers(lua_glue::StateView lua) {
    container_runtime::registerContainers(lua);
}

void shutdownContainers(lua_State* state) noexcept {
    container_runtime::shutdownContainers(state);
}

}  // namespace ludork::standard::binding
