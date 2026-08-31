#include "LatentRuntime.hpp"

#include "ExecutionState.hpp"

#include <utility>

namespace ludork::engine::graph_detail {

bool tryLockExecution(ExecutionState& state, const std::string& key) {
    if (state.locked[key]) {
        return false;
    }
    state.locked[key] = true;
    return true;
}

bool isExecutionLocked(const ExecutionState& state, const std::string& key) {
    const auto locked = state.locked.find(key);
    return locked != state.locked.end() && locked->second;
}

void addLatent(ExecutionState& state, const std::string& key) {
    ++state.latentPendingCount[key];
}

void resolveLatent(ExecutionState& state, const std::string& key) {
    const auto count = state.latentPendingCount.find(key);
    if (count != state.latentPendingCount.end() && count->second > 0) {
        --count->second;
    }
}

std::size_t latentCount(const ExecutionState& state, const std::string& key) {
    const auto count = state.latentPendingCount.find(key);
    return count == state.latentPendingCount.end() ? 0 : count->second;
}

void addCompletionCallback(ExecutionState& state, const std::string& key,
                           std::function<void()> callback) {
    if (callback) {
        state.completionCallbacks[key].push_back(std::move(callback));
    }
}

std::vector<std::function<void()>> completeExecution(ExecutionState& state,
                                                     const std::string& key) {
    state.locked[key] = false;
    if (latentCount(state, key) > 0) {
        return {};
    }
    const auto callbacks = state.completionCallbacks.find(key);
    if (callbacks == state.completionCallbacks.end()) {
        return {};
    }
    std::vector<std::function<void()>> pending = std::move(callbacks->second);
    state.completionCallbacks.erase(callbacks);
    return pending;
}

}  // namespace ludork::engine::graph_detail
