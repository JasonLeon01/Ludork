#include <Runtime/NodeGraph/LatentManager.hpp>

#include <Runtime/NodeGraph/Graph.hpp>
#include <Runtime/NodeGraph/Node.hpp>
#include <Runtime/NodeGraph/NodeGraphRuntime.hpp>
#include <Runtime/RuntimeReflection.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
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
    NodeGraphConditionResult result =
        nodeGraphRuntime().evaluateCondition(condition);
    return {std::move(result.result.values), result.result.count,
            result.finished};
}

bool runtimeEqual(const RuntimeValue& left, const RuntimeValue& right) {
    return runtimeReflection().equal(left, right);
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
    LocalGraphScope(Graph& graph, RuntimeIdentityPtr replacement,
                    std::string eventKey)
        : graph_(graph),
          previous_(graph.getLocalGraph()),
          context_(replacement),
          eventKey_(std::move(eventKey)) {
        graph_.setLocalGraph(std::move(replacement));
        if (context_ == nullptr) {
            return;
        }
        try {
            previousContextGraph_ =
                nodeGraphRuntime().getContextValue(context_, "__graph__");
            nodeGraphRuntime().setContextValue(context_, "__graph__",
                                               graph_.getGraphContext());
            contextGraphSet_ = true;
        } catch (...) {
            graph_.setLocalGraph(std::move(previous_));
            throw;
        }
    }

    ~LocalGraphScope() noexcept {
        if (contextGraphSet_) {
            try {
                nodeGraphRuntime().setContextValue(context_, "__graph__",
                                                   previousContextGraph_);
            } catch (const std::exception& error) {
                std::cerr << "WARNING:Latent event '" << eventKey_
                          << "' failed to restore context key '__graph__': "
                          << error.what() << '\n';
            } catch (...) {
                std::cerr << "WARNING:Latent event '" << eventKey_
                          << "' failed to restore context key '__graph__': "
                             "unknown error\n";
            }
        }
        graph_.setLocalGraph(std::move(previous_));
    }

    LocalGraphScope(const LocalGraphScope&) = delete;
    LocalGraphScope& operator=(const LocalGraphScope&) = delete;

private:
    Graph& graph_;
    RuntimeIdentityPtr previous_;
    RuntimeIdentityPtr context_;
    std::string eventKey_;
    RuntimeValue previousContextGraph_;
    bool contextGraphSet_ = false;
};

}  // namespace

const std::shared_ptr<LatentManager> latentManagerInstance =
    std::make_shared<LatentManager>();

LatentManager& latentManager() {
    return *latentManagerInstance;
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
            LocalGraphScope localGraph(*graph, entry->localRef, entry->key);
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
