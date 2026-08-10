#include <NodeGraph/LatentManager.hpp>

#include <Input/InputService.hpp>
#include <NodeGraph/Graph.hpp>
#include <NodeGraph/Node.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {
const RuntimeValue* mapValue(const RuntimeValue::Map& map,
                             const std::string& name) {
    const auto iterator = map.find(name);
    return iterator == map.end() ? nullptr : &iterator->second;
}

std::size_t countValue(const RuntimeValue* value, std::size_t fallback) {
    if (value == nullptr) {
        return fallback;
    }
    if (const std::int64_t* integer = value->getIf<std::int64_t>()) {
        return *integer < 0 ? fallback : static_cast<std::size_t>(*integer);
    }
    if (const double* number = value->getIf<double>()) {
        if (!std::isfinite(*number) || *number < 0.0 ||
            std::floor(*number) != *number) {
            return fallback;
        }
        return static_cast<std::size_t>(*number);
    }
    return fallback;
}

bool boolValue(const RuntimeValue* value, bool fallback) {
    if (value == nullptr) {
        return fallback;
    }
    const bool* flag = value->getIf<bool>();
    return flag == nullptr ? fallback : *flag;
}

struct ConditionResult {
    RuntimeValue::Array values;
    std::size_t count = 0;
    bool finished = true;
};

class UpdateScope {
public:
    explicit UpdateScope(bool& updating) : updating_(updating) {
        updating_ = true;
    }

    ~UpdateScope() {
        updating_ = false;
    }

    UpdateScope(const UpdateScope&) = delete;
    UpdateScope& operator=(const UpdateScope&) = delete;

private:
    bool& updating_;
};

ConditionResult pollCondition(const RuntimeIdentityPtr& condition) {
    const std::vector<RuntimeValue> result =
        resolveRuntime("nodegraph.condition", {RuntimeValue(condition)});
    if (result.empty()) {
        throw std::runtime_error(
            "Latent condition did not return a descriptor");
    }
    const RuntimeValue::Map* descriptor =
        result.front().getIf<RuntimeValue::Map>();
    if (descriptor == nullptr) {
        throw std::runtime_error("Latent condition descriptor must be a map");
    }

    ConditionResult parsed;
    const RuntimeValue* valuesValue = mapValue(*descriptor, "values");
    if (valuesValue != nullptr) {
        const RuntimeValue::Array* values =
            valuesValue->getIf<RuntimeValue::Array>();
        if (values == nullptr) {
            throw std::runtime_error(
                "Latent condition values must be an array");
        }
        parsed.values = *values;
    }
    parsed.count =
        countValue(mapValue(*descriptor, "count"), parsed.values.size());
    parsed.finished = boolValue(mapValue(*descriptor, "finished"), true);
    return parsed;
}

bool nativeRuntimeEqual(const RuntimeValue& left, const RuntimeValue& right) {
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
    if (const RuntimeValue::Object* leftValue =
            left.getIf<RuntimeValue::Object>()) {
        const RuntimeValue::Object* rightValue =
            right.getIf<RuntimeValue::Object>();
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
    return false;
}

bool runtimeEqual(const RuntimeValue& left, const RuntimeValue& right) {
    const std::vector<RuntimeValue> reflected =
        resolveRuntime("reflect.equal", {left, right});
    if (!reflected.empty()) {
        const bool* equal = reflected.front().getIf<bool>();
        if (equal != nullptr) {
            return *equal;
        }
    }
    return nativeRuntimeEqual(left, right);
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

std::vector<int> latentExecIndexes(const NodeMemberMetadata& metadata,
                                   const ConditionResult& condition) {
    std::vector<int> result;
    for (std::size_t valueIndex = 0; valueIndex < condition.count;
         ++valueIndex) {
        const RuntimeValue value = valueIndex < condition.values.size()
                                       ? condition.values[valueIndex]
                                       : RuntimeValue();
        for (std::size_t stateIndex = 0;
             stateIndex < metadata.latentStates.size(); ++stateIndex) {
            const NodeNamedValues& state = metadata.latentStates[stateIndex];
            const bool matched = std::any_of(
                state.values.begin(), state.values.end(),
                [&value](const RuntimeValue& candidate) {
                    return runtimeEqual(value, normaliseMatchValue(candidate));
                });
            if (matched &&
                std::find(result.begin(), result.end(),
                          static_cast<int>(stateIndex)) == result.end()) {
                result.push_back(static_cast<int>(stateIndex));
            }
        }
    }
    return result;
}

class LocalGraphScope {
public:
    LocalGraphScope(Graph& graph, RuntimeIdentityPtr replacement)
        : graph_(graph),
          previous_(graph.getLocalGraph()),
          context_(replacement) {
        graph_.setLocalGraph(std::move(replacement));
        if (context_.isNil()) {
            return;
        }
        try {
            const std::vector<RuntimeValue> previousGraph =
                resolveRuntime("nodegraph.context",
                               {context_, RuntimeValue(std::string("get")),
                                RuntimeValue(std::string("__graph__"))});
            previousContextGraph_ =
                previousGraph.empty() ? RuntimeValue() : previousGraph.front();
            resolveRuntime("nodegraph.context",
                           {context_, RuntimeValue(std::string("set")),
                            RuntimeValue(std::string("__graph__")),
                            graph_.getGraphContext()});
            contextGraphSet_ = true;
        } catch (...) {
            graph_.setLocalGraph(std::move(previous_));
            throw;
        }
    }

    ~LocalGraphScope() {
        if (contextGraphSet_) {
            try {
                resolveRuntime("nodegraph.context",
                               {context_, RuntimeValue(std::string("set")),
                                RuntimeValue(std::string("__graph__")),
                                previousContextGraph_});
            } catch (...) {}
        }
        graph_.setLocalGraph(std::move(previous_));
    }

    LocalGraphScope(const LocalGraphScope&) = delete;
    LocalGraphScope& operator=(const LocalGraphScope&) = delete;

private:
    Graph& graph_;
    RuntimeIdentityPtr previous_;
    RuntimeValue context_;
    RuntimeValue previousContextGraph_;
    bool contextGraphSet_ = false;
};

}  // namespace

const std::shared_ptr<LatentManager> latentManagerInstance =
    std::make_shared<LatentManager>();

LatentManager& latentManager() {
    return *latentManagerInstance;
}

void initializeLatent() {
    LatentManager& manager = latentManager();
    if (manager.isInitialised()) {
        return;
    }
    manager.setInitialised(true);
    inputService().setFrameCompletionCallback([]() {
        latentManager().update();
    });
}

void shutdownLatent() noexcept {
    inputService().setFrameCompletionCallback({});
    latentManager().clear();
    latentManager().setInitialised(false);
}

void LatentManager::add(const std::shared_ptr<Graph>& graph,
                        const std::string& key, RuntimeIdentityPtr condition,
                        RuntimeIdentityPtr localRef, int index,
                        NodeCache cache) {
    if (graph == nullptr) {
        throw std::invalid_argument("Latent graph cannot be null");
    }
    if (condition == nullptr) {
        throw std::invalid_argument("Latent condition cannot be null");
    }
    graph->onLatentAdded(key);
    std::shared_ptr<Entry> entry = std::make_shared<Entry>();
    entry->graph = graph;
    entry->key = key;
    entry->condition = std::move(condition);
    entry->localRef = std::move(localRef);
    entry->index = index;
    entry->cache = std::move(cache);
    entries_.push_back(std::move(entry));
}

void LatentManager::update() {
    if (updating_) {
        return;
    }
    const UpdateScope updateScope(updating_);
    const std::vector<std::shared_ptr<Entry>> snapshot = entries_;
    for (const std::shared_ptr<Entry>& entry : snapshot) {
        if (std::find(entries_.begin(), entries_.end(), entry) ==
            entries_.end()) {
            continue;
        }

        const std::shared_ptr<Graph> graph = entry->graph.lock();
        if (graph == nullptr) {
            removeLatentsForNode(nullptr, entry->key, entry->index);
            continue;
        }

        const ConditionResult condition = pollCondition(entry->condition);
        const std::vector<std::shared_ptr<Node>> nodes =
            graph->getNodes(entry->key);
        if (entry->index < 0 ||
            static_cast<std::size_t>(entry->index) >= nodes.size() ||
            nodes[static_cast<std::size_t>(entry->index)] == nullptr) {
            throw std::out_of_range("Latent node index is out of range");
        }
        const NodeMemberMetadata& metadata =
            nodes[static_cast<std::size_t>(entry->index)]->getMemberMetadata();
        const std::vector<int> execIndexes =
            latentExecIndexes(metadata, condition);
        if (execIndexes.empty()) {
            continue;
        }

        {
            LocalGraphScope localGraph(*graph, entry->localRef);
            const Graph::PinNexts& nexts =
                graph->getNodeNexts(entry->key, entry->index);
            for (const int execIndex : execIndexes) {
                const auto next = nexts.find(execIndex);
                if (next == nexts.end()) {
                    continue;
                }
                const int* nextNode = std::get_if<int>(&next->second.node);
                if (nextNode == nullptr) {
                    throw std::runtime_error(
                        "Latent execution target must be a node index");
                }
                graph->executeResult(entry->key, *nextNode, 1000000,
                                     &entry->cache);
            }
        }

        if (condition.finished) {
            removeLatentsForNode(graph, entry->key, entry->index);
            graph->onLatentResolved(entry->key);
            if (graph->getLatentPendingCount(entry->key) == 0) {
                graph->resumeSuspendedLoops(entry->key);
            }
            graph->completeExecution(entry->key);
        }
    }
}

bool LatentManager::isInitialised() const noexcept {
    return initialised_;
}

void LatentManager::setInitialised(bool value) noexcept {
    initialised_ = value;
}

void LatentManager::clear() noexcept {
    entries_.clear();
}

void LatentManager::removeLatentsForNode(const std::shared_ptr<Graph>& graph,
                                         const std::string& key, int index) {
    entries_.erase(
        std::remove_if(
            entries_.begin(), entries_.end(),
            [&graph, &key, index](const std::shared_ptr<Entry>& entry) {
                const std::shared_ptr<Graph> latentGraph = entry->graph.lock();
                if (graph == nullptr) {
                    return latentGraph == nullptr;
                }
                return latentGraph == graph && entry->key == key &&
                       entry->index == index;
            }),
        entries_.end());
}
