#include <Runtime/RuntimeSession.hpp>

#include "RuntimeServiceInternals.hpp"
#include "Blueprint/BlueprintRuntime/BlueprintRuntimeInternal.hpp"
#include "Blueprint/ClassRuntime/ClassRuntimeInternal.hpp"
#include "Components/ComponentRuntimeCache.hpp"
#include "NodeGraph/NodeGraphRuntime/NodeGraphRuntimeInternal.hpp"

#include <Runtime/NodeGraph/LatentManager.hpp>
#include <Runtime/RuntimeProviders.hpp>

#include <stdexcept>

namespace {

constexpr const char* RUNTIME_MODULE_STATE_KEY = "Ludork.Runtime.moduleState";

enum class RuntimeModuleState {
    unattached,
    attached,
    detached,
};

RuntimeModuleState moduleState(lua_State* state) noexcept {
    lua_getfield(state, LUA_REGISTRYINDEX, RUNTIME_MODULE_STATE_KEY);
    const RuntimeModuleState result =
        static_cast<RuntimeModuleState>(lua_tointeger(state, -1));
    lua_pop(state, 1);
    return result;
}

void setModuleState(lua_State* state, RuntimeModuleState value) noexcept {
    if (value == RuntimeModuleState::unattached) {
        lua_pushnil(state);
    } else {
        lua_pushinteger(state, static_cast<lua_Integer>(value));
    }
    lua_setfield(state, LUA_REGISTRYINDEX, RUNTIME_MODULE_STATE_KEY);
}

void clearRuntimeState(lua_State* state) noexcept {
    using namespace ludork::runtime;
    latentManager().clear();
    latentManager().setInitialised(false);
    class_runtime_detail::shutdownClassRuntime(state);
    blueprint_detail::clearBlueprintRuntimeCaches(state);
    node_graph_detail::clearNodeGraphRuntimeCaches(state);
    componentRuntimeCache().clear(state);
    detail::clearRuntimeProviders();
    detail::clearRuntimeCaches(sol::state_view(state));
}

}  // namespace

namespace ludork::runtime {

void initialize(lua_State* state) {
    if (state == nullptr) {
        throw std::invalid_argument("Runtime state must not be null");
    }
    ludork::standard::LuaExecutionScope execution(state);
    if (!execution.active() ||
        ludork::standard::isRuntimeStopping(execution.state())) {
        throw std::runtime_error("Runtime requires a running Lua session");
    }
    state = execution.state();
    switch (moduleState(state)) {
        case RuntimeModuleState::attached:
            return;
        case RuntimeModuleState::detached:
            throw std::runtime_error(
                "Runtime was shut down; create a new Lua session to restart");
        case RuntimeModuleState::unattached:
            break;
    }
    ludork::standard::registerRuntimeCleanup(state, shutdown);
    setModuleState(state, RuntimeModuleState::attached);
    try {
        detail::clearRuntimeCaches(sol::state_view(state));
        detail::clearRuntimeProviders();
        blueprint_detail::clearBlueprintRuntimeCaches(state);
        node_graph_detail::clearNodeGraphRuntimeCaches(state);
        componentRuntimeCache().clear(state);
        class_runtime_detail::initializeClassRuntime(state);
    } catch (...) {
        clearRuntimeState(state);
        setModuleState(state, RuntimeModuleState::unattached);
        throw;
    }
}

void shutdown(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    ludork::standard::LuaExecutionScope execution(state);
    if (!execution.active() ||
        moduleState(execution.state()) != RuntimeModuleState::attached) {
        return;
    }
    state = execution.state();
    clearRuntimeState(state);
    setModuleState(state, RuntimeModuleState::detached);
}

RuntimeScope::RuntimeScope() {
    if (!execution_.active()) {
        throw std::runtime_error(
            "Lua runtime session is unavailable or stopping");
    }
    if (moduleState(execution_.state()) != RuntimeModuleState::attached) {
        throw std::runtime_error("Runtime is not initialized");
    }
}

lua_State* RuntimeScope::state() const noexcept {
    return execution_.state();
}

}  // namespace ludork::runtime
