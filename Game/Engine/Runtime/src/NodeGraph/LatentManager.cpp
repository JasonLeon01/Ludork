#include <Runtime/NodeGraph/LatentManager.hpp>
#include "LatentManagerImpl.hpp"

#include <Runtime/NodeGraph/Graph.hpp>
#include <Runtime/NodeGraph/Node.hpp>
#include "NodeGraphRuntime/NodeGraphRuntimeInternal.hpp"
#include <Runtime/RuntimeReflection.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
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
                                   const RuntimeValue& value) {
    std::vector<int> result;
    for (std::size_t stateIndex = 0; stateIndex < metadata.latentStates.size();
         ++stateIndex) {
        const NodeNamedValues& state = metadata.latentStates[stateIndex];
        const bool matched = std::any_of(
            state.values.begin(), state.values.end(),
            [&value](const RuntimeValue& candidate) {
                return runtimeEqual(value, normaliseMatchValue(candidate));
            });
        if (matched) {
            result.push_back(static_cast<int>(stateIndex));
        }
    }
    return result;
}

}  // namespace

namespace ludork::runtime::latent_detail {

UpdateScope::UpdateScope(bool& updating) : updating_(updating) {
    updating_ = true;
}
UpdateScope::~UpdateScope() {
    updating_ = false;
}

LocalGraphScope::LocalGraphScope(Graph& graph, RuntimeIdentityPtr replacement,
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
        ludork::runtime::RuntimeScope scope;
        previousContextGraph_ =
            ludork::runtime::node_graph_detail::getNodeGraphContextValue(
                scope, RuntimeHandle(context_), "__graph__");
        ludork::runtime::node_graph_detail::setNodeGraphContextValue(
            scope, RuntimeHandle(context_), "__graph__",
            graph_.getGraphContext());
        contextGraphSet_ = true;
    } catch (...) {
        graph_.setLocalGraph(std::move(previous_));
        throw;
    }
}

LocalGraphScope::~LocalGraphScope() noexcept {
    if (contextGraphSet_) {
        try {
            ludork::runtime::RuntimeScope scope;
            ludork::runtime::node_graph_detail::setNodeGraphContextValue(
                scope, RuntimeHandle(context_), "__graph__",
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

}  // namespace ludork::runtime::latent_detail

const std::shared_ptr<LatentManager> latentManagerInstance =
    std::make_shared<LatentManager>();

LatentManager& latentManager() {
    return *latentManagerInstance;
}

void LatentManager::add(const std::shared_ptr<Graph>& graph,
                        const std::string& key,
                        std::shared_ptr<AsyncOperation> operation,
                        RuntimeIdentityPtr localRef, int index,
                        NodeCache cache) {
    if (graph == nullptr) {
        throw std::invalid_argument("Latent graph cannot be null");
    }
    if (operation == nullptr) {
        throw std::invalid_argument("Latent operation cannot be null");
    }
    graph->onLatentAdded(key);
    std::shared_ptr<Entry> entry = std::make_shared<Entry>();
    entry->graph = graph;
    entry->key = key;
    entry->operation = std::move(operation);
    if (entry->operation->getStatus() == "completed") {
        entry->nextEvent = entry->operation->eventCount() - 1;
    }
    entry->localRef = std::move(localRef);
    entry->index = index;
    entry->cache = std::move(cache);
    entries_.push_back(entry);
    const std::weak_ptr<LatentManager> manager = weak_from_this();
    const std::weak_ptr<Entry> registration = entry;
    entry->operation->onCancelled([manager, registration] {
        const std::shared_ptr<LatentManager> activeManager = manager.lock();
        const std::shared_ptr<Entry> activeEntry = registration.lock();
        if (activeManager != nullptr && activeEntry != nullptr) {
            activeManager->cancel(activeEntry->operation);
        }
    });
}

void LatentManager::update() {
    if (updating_) {
        return;
    }
    const ludork::runtime::latent_detail::UpdateScope updateScope(updating_);
    AsyncOperation::updateAll();
    const std::vector<std::shared_ptr<Entry>> snapshot = entries_;
    for (const std::shared_ptr<Entry>& entry : snapshot) {
        const auto isPending = [this, &entry]() {
            return std::find(entries_.begin(), entries_.end(), entry) !=
                   entries_.end();
        };
        if (!isPending()) {
            continue;
        }

        const std::shared_ptr<Graph> graph = entry->graph.lock();
        if (graph == nullptr) {
            removeLatentsForNode(nullptr, entry->key, entry->index);
            continue;
        }

        if (entry->operation->getStatus() == "cancelled") {
            cancel(entry->operation);
            continue;
        }
        const std::vector<std::shared_ptr<Node>> nodes =
            graph->getNodes(entry->key);
        if (!isPending()) {
            continue;
        }
        if (entry->index < 0 ||
            static_cast<std::size_t>(entry->index) >= nodes.size() ||
            nodes[static_cast<std::size_t>(entry->index)] == nullptr) {
            throw std::out_of_range("Latent node index is out of range");
        }
        const NodeMemberMetadata& metadata =
            nodes[static_cast<std::size_t>(entry->index)]->getMemberMetadata();
        const std::size_t eventCount = entry->operation->eventCount();
        while (entry->nextEvent < eventCount && isPending() &&
               entry->operation->getStatus() != "cancelled") {
            const RuntimeValue value =
                entry->operation->eventAt(entry->nextEvent++);
            const std::vector<int> execIndexes =
                latentExecIndexes(metadata, value);
            ludork::runtime::latent_detail::LocalGraphScope localGraph(
                *graph, entry->localRef, entry->key);
            const Graph::PinNexts& nexts =
                graph->getNodeNexts(entry->key, entry->index);
            for (const int execIndex : execIndexes) {
                if (!isPending() ||
                    entry->operation->getStatus() == "cancelled") {
                    break;
                }
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
        if (entry->operation->getStatus() == "cancelled") {
            cancel(entry->operation);
            continue;
        }

        if (entry->operation->getStatus() == "completed" &&
            entry->nextEvent == entry->operation->eventCount() && isPending()) {
            const std::uint64_t revision = graph->executionRevision(entry->key);
            std::erase(entries_, entry);
            graph->onLatentResolved(entry->key);
            if (graph->getLatentPendingCount(entry->key) == 0) {
                graph->resumeSuspendedLoops(entry->key);
            }
            if (graph->executionRevision(entry->key) == revision) {
                graph->completeExecution(entry->key);
            }
        }
    }
}

void LatentManager::cancel(const std::shared_ptr<AsyncOperation>& operation) {
    if (operation == nullptr) {
        throw std::invalid_argument("Latent operation cannot be null");
    }
    if (operation->isPending()) {
        operation->cancel();
        return;
    }
    if (operation->getStatus() != "cancelled") {
        return;
    }
    std::vector<std::shared_ptr<Entry>> matches;
    std::vector<std::pair<std::shared_ptr<Graph>, std::string>> executions;
    const std::vector<std::shared_ptr<Entry>> snapshot = entries_;
    for (const std::shared_ptr<Entry>& entry : snapshot) {
        if (entry->operation != operation) {
            continue;
        }
        matches.push_back(entry);
        if (const std::shared_ptr<Graph> graph = entry->graph.lock()) {
            const std::pair execution{graph, entry->key};
            if (std::find(executions.begin(), executions.end(), execution) ==
                executions.end()) {
                executions.push_back(execution);
            }
        }
    }
    std::erase_if(entries_, [&matches,
                             &executions](const std::shared_ptr<Entry>& entry) {
        if (std::find(matches.begin(), matches.end(), entry) != matches.end()) {
            return true;
        }
        const std::pair execution{entry->graph.lock(), entry->key};
        return std::find(executions.begin(), executions.end(), execution) !=
               executions.end();
    });
    for (const auto& [graph, key] : executions) {
        graph->cancelExecutionState(key);
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
