#pragma once

#include <NodeGraph/Node.hpp>
#include <NodeGraph/Types.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ludork::engine::graph_detail {

struct LoopFrame {
    std::string key;
    int loopNodeIndex = 0;
    std::vector<NodeResult> remainingResults;
    std::size_t nextResult = 0;
    int bodyStart = 0;
    std::vector<NodeIndex> bodyCacheKeys;
    NodeCache baseCache;
    NodeResult lastResult;
    std::size_t loopSteps = 0;
    std::optional<int> completedNext;
    std::size_t limit = 1000000;
};

struct LoopResult {
    std::optional<int> next;
    NodeResult result;
    std::size_t steps = 0;
};

struct ExecutionState {
    std::unordered_map<std::string, bool> locked;
    std::unordered_map<std::string, std::size_t> latentPendingCount;
    std::unordered_map<std::string, std::vector<std::function<void()>>>
        completionCallbacks;
    std::vector<std::shared_ptr<LoopFrame>> loopFrames;
    std::string doingPartKey;
    bool suspendedByLatent = false;
};

}  // namespace ludork::engine::graph_detail
