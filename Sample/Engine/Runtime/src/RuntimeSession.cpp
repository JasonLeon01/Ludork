#include <Runtime/RuntimeSession.hpp>

#include "RuntimeServiceInternals.hpp"
#include "Blueprint/BlueprintRuntime/BlueprintRuntimeInternal.hpp"
#include "Blueprint/ClassRuntime/ClassRuntimeInternal.hpp"
#include "Components/ComponentRuntimeCache.hpp"
#include "NodeGraph/NodeGraphRuntime/NodeGraphRuntimeInternal.hpp"

#include <Runtime/NodeGraph/LatentManager.hpp>
#include <Runtime/RuntimeProviders.hpp>

#include <atomic>
#include <stdexcept>

namespace {

std::atomic<lua_State*> runtimeState{nullptr};

}  // namespace

namespace ludork::runtime {

void initialize(lua_State* state) {
    if (state == nullptr) {
        throw std::invalid_argument("Runtime state must not be null");
    }
    lua_State* expected = nullptr;
    if (!runtimeState.compare_exchange_strong(expected, state,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire) &&
        expected != state) {
        throw std::runtime_error("Runtime is already initialized");
    }
    sol::state_view lua(state);
    detail::clearRuntimeCaches(lua);
    detail::clearRuntimeProviders();
    blueprint_detail::clearBlueprintRuntimeCaches();
    node_graph_detail::clearNodeGraphRuntimeCaches();
    componentRuntimeCache().clear();
    class_runtime_detail::initializeClassRuntime(state);
}

void shutdown(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    sol::state_view lua(state);
    latentManager().clear();
    latentManager().setInitialised(false);
    class_runtime_detail::shutdownClassRuntime(state);
    blueprint_detail::clearBlueprintRuntimeCaches();
    node_graph_detail::clearNodeGraphRuntimeCaches();
    componentRuntimeCache().clear();
    detail::clearRuntimeProviders();
    detail::clearRuntimeCaches(lua);
    lua_State* expected = state;
    runtimeState.compare_exchange_strong(expected, nullptr,
                                         std::memory_order_release,
                                         std::memory_order_relaxed);
}

RuntimeScope::RuntimeScope()
    : state_(runtimeState.load(std::memory_order_acquire)) {
    if (state_ == nullptr) {
        throw std::runtime_error("Runtime is not initialized");
    }
    execution_.emplace(state_);
    if (!execution_->active()) {
        throw std::runtime_error("Lua runtime session is stopping");
    }
}

lua_State* RuntimeScope::state() const noexcept {
    return state_;
}

}  // namespace ludork::runtime
