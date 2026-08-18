#include <NodeGraph/Graph.hpp>
#include "GraphInternal.hpp"

#include <NodeGraph/LatentManager.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

using namespace ludork::engine::graph_detail;

namespace {

bool runtimeValueEqual(const RuntimeValue& left, const RuntimeValue& right) {
    if (left.isNil() || right.isNil()) {
        return left.isNil() && right.isNil();
    }
    if (const bool* leftValue = left.getIf<bool>()) {
        const bool* rightValue = right.getIf<bool>();
        return rightValue != nullptr && *leftValue == *rightValue;
    }
    if (const std::int64_t* leftValue = left.getIf<std::int64_t>()) {
        if (const std::int64_t* rightValue = right.getIf<std::int64_t>()) {
            return *leftValue == *rightValue;
        }
        if (const double* rightValue = right.getIf<double>()) {
            return static_cast<double>(*leftValue) == *rightValue;
        }
        return false;
    }
    if (const double* leftValue = left.getIf<double>()) {
        if (const double* rightValue = right.getIf<double>()) {
            return *leftValue == *rightValue;
        }
        if (const std::int64_t* rightValue = right.getIf<std::int64_t>()) {
            return *leftValue == static_cast<double>(*rightValue);
        }
        return false;
    }
    if (const std::string* leftValue = left.getIf<std::string>()) {
        const std::string* rightValue = right.getIf<std::string>();
        return rightValue != nullptr && *leftValue == *rightValue;
    }
    if (const RuntimeIdentityPtr* leftValue =
            left.getIf<RuntimeIdentityPtr>()) {
        const RuntimeIdentityPtr* rightValue =
            right.getIf<RuntimeIdentityPtr>();
        if (rightValue == nullptr || *leftValue == nullptr ||
            *rightValue == nullptr) {
            return rightValue != nullptr && *leftValue == *rightValue;
        }
        return (*leftValue)->equals(**rightValue);
    }
    if (const RuntimeValue::Object* leftValue =
            left.getIf<RuntimeValue::Object>()) {
        const RuntimeValue::Object* rightValue =
            right.getIf<RuntimeValue::Object>();
        return rightValue != nullptr && *leftValue == *rightValue;
    }
    return false;
}

RuntimeValue normaliseMatchValue(const RuntimeValue& value) {
    const std::string* text = value.getIf<std::string>();
    if (text == nullptr) {
        return value;
    }
    if (*text == "nil") {
        return RuntimeValue();
    }
    return value;
}

class StringRestore {
public:
    explicit StringRestore(std::string& value)
        : value_(value), previous_(value) {}

    ~StringRestore() {
        value_ = std::move(previous_);
    }

    StringRestore(const StringRestore&) = delete;
    StringRestore& operator=(const StringRestore&) = delete;

private:
    std::string& value_;
    std::string previous_;
};

}  // namespace

RuntimeValue::Array Graph::execute(const std::string& key,
                                   std::optional<int> startNode,
                                   std::optional<std::size_t> limit,
                                   std::optional<RuntimeIdentityPtr> cache) {
    const RuntimeIdentityPtr cacheIdentity = cache.value_or(nullptr);
    if (cacheIdentity == nullptr) {
        return executeResult(key, startNode, limit.value_or(1000000)).values;
    }
    NodeCache cacheValues = decodeNodeCache(cacheIdentity);
    NodeResult result =
        executeResult(key, startNode, limit.value_or(1000000), &cacheValues);
    syncNodeCache(cacheIdentity, cacheValues);
    return result.values;
}

NodeResult Graph::executeResult(const std::string& key,
                                std::optional<int> startNode, std::size_t limit,
                                NodeCache* externalCache) {
    ensureEventInitialised(key);
    suspendedByLatent_ = false;
    doingPartKey_ = key;
    const auto nodeEvent = nodes_.find(key);
    if (nodeEvent == nodes_.end()) {
        throw std::runtime_error("Graph key '" + key + "' not found");
    }
    const auto configuredStart = startNodes.find(key);
    if (configuredStart == startNodes.end()) {
        throw std::runtime_error("Start node for key '" + key + "' not set");
    }
    int current = startNode.value_or(configuredStart->second);
    if (current < 0 ||
        static_cast<std::size_t>(current) >= nodeEvent->second.size()) {
        throw std::runtime_error("startIndex out of range for key '" + key +
                                 "'");
    }

    NodeCache localCache;
    NodeCache& cache = externalCache == nullptr ? localCache : *externalCache;
    std::size_t steps = 0;
    while (true) {
        NodeResult result = executeNodeResult(key, NodeIndex(current), cache);
        const std::shared_ptr<Node>& node = nodeEvent->second[current];
        const NodeMemberMetadata& metadata = node->getMemberMetadata();
        if (metadata.latent || !metadata.latentStates.empty()) {
            RuntimeIdentityPtr condition =
                result.count == 0 || result.values.empty()
                    ? nullptr
                    : identityValue(&result.values.front());
            if (condition == nullptr) {
                throw std::runtime_error(
                    "Latent node did not return a condition");
            }
            if (!latentManager().isInitialised()) {
                throw std::runtime_error(
                    "Engine latent runtime is not initialised");
            }
            std::shared_ptr<Graph> self =
                std::dynamic_pointer_cast<Graph>(shared_from_this());
            latentManager().add(self, key, condition, localGraph, current,
                                cache);
            suspendedByLatent_ = true;
            return result;
        }

        if (metadata.loop || !metadata.loopNode.empty()) {
            LoopResult loop =
                executeLoopNode(key, current, result, cache, limit);
            result = std::move(loop.result);
            steps += loop.steps;
            if (steps >= limit) {
                throw std::runtime_error(
                    "Max steps exceeded while executing graph '" + key + "'");
            }
            if (suspendedByLatent_) {
                return result;
            }
            if (!loop.next.has_value()) {
                return result;
            }
            current = *loop.next;
            continue;
        }

        const PinNexts& nexts = getNodeNexts(key, current);
        if (nexts.empty()) {
            return result;
        }
        std::optional<NodeIndex> chosen;
        if (!metadata.execSplits.empty()) {
            for (std::size_t resultIndex = 0;
                 resultIndex < result.count &&
                 resultIndex < result.values.size() && !chosen.has_value();
                 ++resultIndex) {
                for (std::size_t splitIndex = 0;
                     splitIndex < metadata.execSplits.size(); ++splitIndex) {
                    const NodeNamedValues& split =
                        metadata.execSplits[splitIndex];
                    const auto next = nexts.find(static_cast<int>(splitIndex));
                    if (next == nexts.end()) {
                        continue;
                    }
                    for (const RuntimeValue& candidate : split.values) {
                        if (runtimeValueEqual(result.values[resultIndex],
                                              normaliseMatchValue(candidate))) {
                            chosen = next->second.node;
                            break;
                        }
                    }
                    if (chosen.has_value()) {
                        break;
                    }
                }
            }
            if (!chosen.has_value()) {
                for (std::size_t index = 0; index < metadata.execSplits.size();
                     ++index) {
                    if (metadata.execSplits[index].name != "default") {
                        continue;
                    }
                    const auto next = nexts.find(static_cast<int>(index));
                    if (next != nexts.end()) {
                        chosen = next->second.node;
                    }
                    break;
                }
            }
        } else if (nexts.size() == 1) {
            chosen = nexts.begin()->second.node;
        } else {
            const std::vector<int> pins = sortedPins(nexts);
            chosen = nexts.at(pins.front()).node;
        }

        if (!chosen.has_value()) {
            return result;
        }
        const int* chosenIndex = std::get_if<int>(&*chosen);
        if (chosenIndex == nullptr) {
            return result;
        }
        current = *chosenIndex;
        ++steps;
        if (steps >= limit) {
            throw std::runtime_error(
                "Max steps exceeded while executing graph '" + key + "'");
        }
    }
}

Graph::LoopResult Graph::executeLoopNode(const std::string& key, int nodeIndex,
                                         const NodeResult& controlResult,
                                         NodeCache& cache, std::size_t limit) {
    const std::shared_ptr<Node>& node = nodes_.at(key).at(nodeIndex);
    const NodeMemberMetadata& metadata = node->getMemberMetadata();
    const PinNexts& nexts = getNodeNexts(key, nodeIndex);
    const std::optional<int> bodyPin =
        getNamedExecPinIndex(metadata, "LoopBody");
    const std::optional<int> completedPin =
        getNamedExecPinIndex(metadata, "Completed");
    const NodeSource* body = nullptr;
    const NodeSource* completed = nullptr;
    if (bodyPin.has_value()) {
        const auto found = nexts.find(*bodyPin);
        if (found != nexts.end()) {
            body = &found->second;
        }
    }
    if (completedPin.has_value()) {
        const auto found = nexts.find(*completedPin);
        if (found != nexts.end()) {
            completed = &found->second;
        }
    }

    LoopResult loop;
    loop.result = getLoopEmptyResult(metadata);
    if (body != nullptr) {
        const int* bodyStart = std::get_if<int>(&body->node);
        if (bodyStart != nullptr) {
            const std::vector<NodeIndex> cacheKeys =
                getLoopBodyCacheKeys(key, nodeIndex, *bodyStart);
            std::vector<NodeResult> iterations =
                iterateLoopResults(metadata, controlResult);
            for (std::size_t index = 0; index < iterations.size(); ++index) {
                const std::size_t frameBaseIndex = loopFrames_.size();
                NodeResult iteration = iterations[index];
                const bool suspended =
                    runLoopBodyIteration(key, nodeIndex, *bodyStart, cacheKeys,
                                         cache, iteration, limit);
                loop.result = std::move(iteration);
                ++loop.steps;
                if (loop.steps >= limit) {
                    throw std::runtime_error(
                        "Max steps exceeded while executing graph '" + key +
                        "'");
                }
                if (suspended) {
                    std::shared_ptr<LoopFrame> frame =
                        std::make_shared<LoopFrame>();
                    frame->key = key;
                    frame->loopNodeIndex = nodeIndex;
                    frame->remainingResults = std::move(iterations);
                    frame->nextResult = index + 1;
                    frame->bodyStart = *bodyStart;
                    frame->bodyCacheKeys = cacheKeys;
                    frame->baseCache = cache;
                    frame->lastResult = loop.result;
                    frame->loopSteps = loop.steps;
                    if (completed != nullptr) {
                        if (const int* completedStart =
                                std::get_if<int>(&completed->node)) {
                            frame->completedNext = *completedStart;
                        }
                    }
                    frame->limit = limit;
                    loopFrames_.insert(
                        loopFrames_.begin() +
                            static_cast<std::ptrdiff_t>(frameBaseIndex),
                        std::move(frame));
                    suspendedByLatent_ = true;
                    return loop;
                }
            }
        }
    }

    cache[NodeIndex(nodeIndex)] = loop.result;
    if (completed != nullptr) {
        if (const int* completedStart = std::get_if<int>(&completed->node)) {
            loop.next = *completedStart;
        }
    }
    return loop;
}

bool Graph::runLoopBodyIteration(const std::string& key, int loopNodeIndex,
                                 int bodyStart,
                                 const std::vector<NodeIndex>& bodyCacheKeys,
                                 const NodeCache& baseCache,
                                 const NodeResult& loopResult,
                                 std::size_t limit) {
    NodeCache iterationCache = baseCache;
    for (const NodeIndex& cacheKey : bodyCacheKeys) {
        iterationCache.erase(cacheKey);
    }
    iterationCache[NodeIndex(loopNodeIndex)] = loopResult;
    suspendedByLatent_ = false;
    StringRestore partKeyRestore(doingPartKey_);
    executeResult(key, bodyStart, limit, &iterationCache);
    return suspendedByLatent_;
}

std::optional<int> Graph::getNamedExecPinIndex(
    const NodeMemberMetadata& metadata, const std::string& pinName) const {
    for (std::size_t index = 0; index < metadata.execSplits.size(); ++index) {
        if (metadata.execSplits[index].name == pinName) {
            return static_cast<int>(index);
        }
    }
    return std::nullopt;
}

NodeResult Graph::getLoopEmptyResult(const NodeMemberMetadata& metadata) const {
    if (metadata.loopNode == "ForEach") {
        return NodeResult{{RuntimeValue(), RuntimeValue(std::int64_t{-1})}, 2};
    }
    return NodeResult{{RuntimeValue(std::int64_t{0})}, 1};
}

std::vector<NodeResult> Graph::iterateLoopResults(
    const NodeMemberMetadata& metadata, const NodeResult& controlResult) const {
    std::vector<NodeResult> result;
    if (metadata.loopNode == "ForEach") {
        if (controlResult.count == 0 || controlResult.values.empty()) {
            return result;
        }
        if (const RuntimeValue::Array* items =
                controlResult.values.front().getIf<RuntimeValue::Array>()) {
            result.reserve(items->size());
            for (std::size_t index = 0; index < items->size(); ++index) {
                result.push_back(
                    NodeResult{{(*items)[index],
                                RuntimeValue(static_cast<std::int64_t>(index))},
                               2});
            }
        } else if (const RuntimeValue::Map* items =
                       controlResult.values.front()
                           .getIf<RuntimeValue::Map>()) {
            result.reserve(items->size());
            std::size_t index = 0;
            for (const auto& [_, value] : *items) {
                result.push_back(NodeResult{
                    {value, RuntimeValue(static_cast<std::int64_t>(index))},
                    2});
                ++index;
            }
        }
        return result;
    }
    if (metadata.loopNode != "ForLoop") {
        return result;
    }

    auto integerAt = [&controlResult](std::size_t index,
                                      std::int64_t fallback) {
        if (index >= controlResult.count ||
            index >= controlResult.values.size()) {
            return fallback;
        }
        const RuntimeValue& value = controlResult.values[index];
        if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
            return *integer;
        }
        if (const double* number = value.getIf<double>()) {
            return static_cast<std::int64_t>(std::floor(*number));
        }
        return fallback;
    };
    const std::int64_t first = integerAt(0, 0);
    const std::int64_t last = integerAt(1, 0);
    const std::int64_t step = integerAt(2, 1);
    if (step == 0) {
        throw std::runtime_error("ForLoop step cannot be 0");
    }
    for (std::int64_t current = first;
         step > 0 ? current <= last : current >= last; current += step) {
        result.push_back(NodeResult{{RuntimeValue(current)}, 1});
        if ((step > 0 &&
             current > std::numeric_limits<std::int64_t>::max() - step) ||
            (step < 0 &&
             current < std::numeric_limits<std::int64_t>::min() - step)) {
            break;
        }
    }
    return result;
}

std::vector<NodeIndex> Graph::getLoopBodyCacheKeys(const std::string& key,
                                                   int loopNodeIndex,
                                                   int startNode) const {
    std::set<NodeIndex> result;
    std::set<NodeIndex> dependencies;
    const std::vector<NodeIndex> loopDependencies =
        getRelyNodeIndexList(key, NodeIndex(loopNodeIndex));
    std::set<NodeIndex> preserved(loopDependencies.begin(),
                                  loopDependencies.end());
    preserved.insert(NodeIndex(loopNodeIndex));
    std::set<int> visited;
    std::vector<int> stack{startNode};
    while (!stack.empty()) {
        const int nodeIndex = stack.back();
        stack.pop_back();
        if (!visited.insert(nodeIndex).second) {
            continue;
        }
        result.insert(NodeIndex(nodeIndex));
        for (const NodeIndex& rely :
             getRelyNodeIndexList(key, NodeIndex(nodeIndex))) {
            dependencies.insert(rely);
        }
        for (const auto& [_, next] : getNodeNexts(key, nodeIndex)) {
            if (const int* nextIndex = std::get_if<int>(&next.node)) {
                stack.push_back(*nextIndex);
            }
        }
    }
    for (const NodeIndex& dependency : dependencies) {
        const int* dependencyIndex = std::get_if<int>(&dependency);
        if (dependencyIndex == nullptr ||
            visited.find(*dependencyIndex) != visited.end() ||
            *dependencyIndex < 0 ||
            static_cast<std::size_t>(*dependencyIndex) >=
                nodes_.at(key).size()) {
            continue;
        }
        const NodeMemberMetadata& metadata =
            nodes_.at(key)[*dependencyIndex]->getMemberMetadata();
        if (!metadata.loop && metadata.loopNode.empty()) {
            continue;
        }
        preserved.insert(dependency);
        const std::vector<NodeIndex> dependencyInputs =
            getRelyNodeIndexList(key, dependency);
        preserved.insert(dependencyInputs.begin(), dependencyInputs.end());
    }
    for (const NodeIndex& dependency : dependencies) {
        if (preserved.find(dependency) == preserved.end()) {
            result.insert(dependency);
        }
    }
    return std::vector<NodeIndex>(result.begin(), result.end());
}

RuntimeValue::Array Graph::executeNode(
    const std::string& key, const NodeIndex& nodeIndex,
    std::optional<RuntimeIdentityPtr> cache) {
    const RuntimeIdentityPtr cacheIdentity = cache.value_or(nullptr);
    NodeCache cacheValues = decodeNodeCache(cacheIdentity);
    NodeResult result = executeNodeResult(key, nodeIndex, cacheValues);
    syncNodeCache(cacheIdentity, cacheValues);
    return result.values;
}

NodeResult Graph::executeNodeResult(const std::string& key,
                                    const NodeIndex& nodeIndex,
                                    NodeCache& cache) {
    ensureEventInitialised(key);
    const auto cached = cache.find(nodeIndex);
    if (cached != cache.end()) {
        return cached->second;
    }

    if (const std::string* defaultNode = std::get_if<std::string>(&nodeIndex)) {
        if (defaultNode->rfind("default_", 0) != 0) {
            throw std::runtime_error("Unknown graph parameter node '" +
                                     *defaultNode + "'");
        }
        const std::string suffix = defaultNode->substr(8);
        std::size_t consumed = 0;
        const int parameterIndex = std::stoi(suffix, &consumed);
        RuntimeValue value;
        const EventParams& parameters = eventParams();
        const auto event = parameters.find(key);
        if (consumed == suffix.size() && parameterIndex >= 0 &&
            event != parameters.end() &&
            static_cast<std::size_t>(parameterIndex) < event->second.size()) {
            const std::string& name = event->second[parameterIndex];
            value = contextValue("__" + name + "__");
            if (value.isNil()) {
                value = contextValue(name);
            }
        }
        NodeResult result{{value}, 1};
        cache.emplace(nodeIndex, result);
        return result;
    }

    const int index = std::get<int>(nodeIndex);
    const auto eventNodes = nodes_.find(key);
    if (eventNodes == nodes_.end() || index < 0 ||
        static_cast<std::size_t>(index) >= eventNodes->second.size()) {
        throw std::runtime_error("Node index out of range for graph '" + key +
                                 "'");
    }

    Node::InputPinMap replacements;
    const RelyMap& relies = nodeRely();
    const auto relyEvent = relies.find(key);
    if (relyEvent != relies.end()) {
        const auto dependencies = relyEvent->second.find(index);
        if (dependencies != relyEvent->second.end()) {
            for (const int inputPin : sortedPins(dependencies->second)) {
                const NodeSource& source = dependencies->second.at(inputPin);
                const NodeResult upstream =
                    executeNodeResult(key, source.node, cache);
                if (source.pin < 0 ||
                    static_cast<std::size_t>(source.pin) >= upstream.count ||
                    static_cast<std::size_t>(source.pin) >=
                        upstream.values.size()) {
                    throw std::runtime_error(
                        "Output pin out of range for graph node");
                }
                replacements[inputPin] =
                    upstream.values[static_cast<std::size_t>(source.pin)];
            }
        }
    }

    NodeResult result = eventNodes->second[index]->executeResult(replacements);
    cache.emplace(nodeIndex, result);
    return result;
}

void Graph::resumeSuspendedLoops(const std::string& key) {
    while (!loopFrames_.empty()) {
        const std::shared_ptr<LoopFrame> frame = loopFrames_.back();
        if (frame == nullptr || frame->key != key) {
            return;
        }
        while (frame->nextResult < frame->remainingResults.size()) {
            NodeResult loopResult = frame->remainingResults[frame->nextResult];
            ++frame->nextResult;
            const bool suspended = runLoopBodyIteration(
                key, frame->loopNodeIndex, frame->bodyStart,
                frame->bodyCacheKeys, frame->baseCache, loopResult,
                frame->limit);
            frame->lastResult = std::move(loopResult);
            ++frame->loopSteps;
            if (frame->loopSteps >= frame->limit) {
                throw std::runtime_error(
                    "Max steps exceeded while executing graph '" + key + "'");
            }
            if (suspended) {
                return;
            }
        }

        if (loopFrames_.back() != frame) {
            return;
        }
        loopFrames_.pop_back();
        frame->baseCache[NodeIndex(frame->loopNodeIndex)] = frame->lastResult;
        if (!frame->completedNext.has_value()) {
            continue;
        }
        suspendedByLatent_ = false;
        StringRestore partKeyRestore(doingPartKey_);
        executeResult(key, *frame->completedNext, frame->limit,
                      &frame->baseCache);
        if (suspendedByLatent_) {
            return;
        }
    }
}

void Graph::completeExecution(const std::string& key) {
    executionLocked_[key] = false;
    if (getLatentPendingCount(key) > 0) {
        return;
    }
    auto callbacks = executionCompleteCallbacks_.find(key);
    if (callbacks == executionCompleteCallbacks_.end()) {
        return;
    }
    std::vector<std::function<void()>> pending = std::move(callbacks->second);
    executionCompleteCallbacks_.erase(callbacks);
    for (const std::function<void()>& callback : pending) {
        callback();
    }
}
