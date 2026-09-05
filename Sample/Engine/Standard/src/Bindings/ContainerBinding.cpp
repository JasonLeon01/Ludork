#include "Bindings.hpp"

#include "Runtime/ContainerRuntime.hpp"

#include <sol2/sol.hpp>

namespace ludork::standard::binding {

void registerContainers(sol::state_view lua) {
    container_runtime::registerContainers(lua);
}

void shutdownContainers(lua_State* state) noexcept {
    container_runtime::shutdownContainers(state);
}

}  // namespace ludork::standard::binding
