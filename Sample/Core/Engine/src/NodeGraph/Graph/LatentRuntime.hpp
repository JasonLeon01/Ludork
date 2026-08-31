#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace ludork::engine::graph_detail {

struct ExecutionState;

bool tryLockExecution(ExecutionState& state, const std::string& key);
bool isExecutionLocked(const ExecutionState& state, const std::string& key);
void addLatent(ExecutionState& state, const std::string& key);
void resolveLatent(ExecutionState& state, const std::string& key);
std::size_t latentCount(const ExecutionState& state, const std::string& key);
void addCompletionCallback(ExecutionState& state, const std::string& key,
                           std::function<void()> callback);
std::vector<std::function<void()>> completeExecution(ExecutionState& state,
                                                     const std::string& key);

}  // namespace ludork::engine::graph_detail
