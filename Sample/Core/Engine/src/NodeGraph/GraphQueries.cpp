#include <NodeGraph/Graph.hpp>
#include "GraphInternal.hpp"

#include <NodeGraph/LatentManager.hpp>
#include <Runtime/NodeGraphRuntime.hpp>
#include <Runtime/RuntimeValueServices.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <utility>

using namespace ludork::engine::graph_detail;

namespace {

RuntimeValue runtimeMember(const RuntimeValue& value, const std::string& name) {
    return ludork::engine::runtime_services::invokeFirst(
        "reflect.get", {value, RuntimeValue(name)});
}

RuntimeIdentityPtr callableWithin(const RuntimeValue& value,
                                  const std::string& path) {
    RuntimeValue current = value;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t separator = path.find('.', start);
        const std::size_t end =
            separator == std::string::npos ? path.size() : separator;
        if (end > start) {
            current = runtimeMember(current, path.substr(start, end - start));
        }
        if (current.isNil()) {
            return nullptr;
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
    return identityValue(&current);
}

}  // namespace

namespace ludork::engine::graph_detail {

std::vector<int> sortedPins(
    const std::unordered_map<int, Graph::NodeSource>& pins) {
    std::vector<int> result;
    result.reserve(pins.size());
    for (const auto& [pin, _] : pins) {
        result.push_back(pin);
    }
    std::sort(result.begin(), result.end());
    return result;
}

NodeCache decodeNodeCache(const RuntimeIdentityPtr& cacheIdentity) {
    return cacheIdentity == nullptr
               ? NodeCache{}
               : nodeGraphRuntime().readCache(cacheIdentity);
}

void syncNodeCache(const RuntimeIdentityPtr& cacheIdentity,
                   const NodeCache& cache) {
    if (cacheIdentity == nullptr) {
        return;
    }
    nodeGraphRuntime().writeCache(cacheIdentity, cache);
}

}  // namespace ludork::engine::graph_detail

std::vector<NodeIndex> Graph::getRelyNodeIndexList(
    const std::string& key, const NodeIndex& nodeIndex) const {
    const_cast<Graph*>(this)->ensureEventInitialised(key);
    std::vector<NodeIndex> result;
    std::set<NodeIndex> visited;
    std::vector<NodeIndex> order;
    const RelyMap& relies = nodeRely();
    const auto event = relies.find(key);
    if (event == relies.end()) {
        return result;
    }

    const auto visit = [&](const auto& self, const NodeIndex& current) -> void {
        if (!visited.insert(current).second) {
            return;
        }
        if (const std::string* name = std::get_if<std::string>(&current);
            name != nullptr && name->rfind("default_", 0) == 0) {
            return;
        }
        const int* currentIndex = std::get_if<int>(&current);
        if (currentIndex == nullptr) {
            return;
        }
        const auto dependencies = event->second.find(*currentIndex);
        if (dependencies == event->second.end()) {
            return;
        }
        for (const int inputPin : sortedPins(dependencies->second)) {
            const NodeIndex& left = dependencies->second.at(inputPin).node;
            self(self, left);
            if (std::find(order.begin(), order.end(), left) == order.end()) {
                order.push_back(left);
            }
        }
    };
    visit(visit, nodeIndex);
    return order;
}

std::vector<std::shared_ptr<Node>> Graph::getNodes(
    const std::string& key) const {
    const_cast<Graph*>(this)->ensureEventInitialised(key);
    const auto event = nodes_.find(key);
    return event == nodes_.end() ? std::vector<std::shared_ptr<Node>>{}
                                 : event->second;
}

RuntimeIdentityPtr Graph::getFunctionFromModule(const RuntimeValue& module,
                                                const std::string& path) const {
    return callableWithin(module, path);
}

RuntimeIdentityPtr Graph::getFunctionFromObject(const RuntimeValue& object,
                                                const std::string& path) const {
    return callableWithin(object, path);
}

bool Graph::hasKey(const std::string& key) const {
    const DataNodeMap& events = dataNodes();
    return events.find(key) != events.end();
}

bool Graph::hasExecutableEvent(const std::string& key) const {
    const DataNodeMap& events = dataNodes();
    const auto event = events.find(key);
    const auto start = startNodes.find(key);
    return event != events.end() && start != startNodes.end() &&
           start->second >= 0 &&
           static_cast<std::size_t>(start->second) < event->second.size();
}

bool Graph::tryLockExecution(const std::string& key) {
    if (executionLocked_[key]) {
        return false;
    }
    executionLocked_[key] = true;
    return true;
}

bool Graph::isExecutionLocked(const std::string& key) const {
    const auto locked = executionLocked_.find(key);
    return locked != executionLocked_.end() && locked->second;
}

void Graph::onLatentAdded(const std::string& key) {
    ++latentPendingCount_[key];
}

void Graph::onLatentResolved(const std::string& key) {
    const auto count = latentPendingCount_.find(key);
    if (count != latentPendingCount_.end() && count->second > 0) {
        --count->second;
    }
}

std::size_t Graph::getLatentPendingCount(const std::string& key) const {
    const auto count = latentPendingCount_.find(key);
    return count == latentPendingCount_.end() ? 0 : count->second;
}

void Graph::addExecutionCompleteCallback(const std::string& key,
                                         std::function<void()> callback) {
    if (callback) {
        executionCompleteCallbacks_[key].push_back(std::move(callback));
    }
}

RuntimeValue::Map Graph::asDict() const {
    const_cast<Graph*>(this)->ensureInitialised();
    RuntimeValue::Map result;
    if (parentClassName != "NOT_WRITTEN") {
        result.emplace("parent", RuntimeValue(parentClassName));
    }
    RuntimeValue::Map nodeGraph;
    const LinkMap& graphLinks = links();
    for (const auto& [key, nodes] : nodes_) {
        RuntimeValue::Array serialisedNodes;
        serialisedNodes.reserve(nodes.size());
        for (const std::shared_ptr<Node>& node : nodes) {
            serialisedNodes.emplace_back(node->asDict());
        }

        RuntimeValue::Array serialisedLinks;
        const auto eventLinks = graphLinks.find(key);
        if (eventLinks != graphLinks.end()) {
            serialisedLinks.reserve(eventLinks->second.size());
            for (const GraphLink& link : eventLinks->second) {
                serialisedLinks.emplace_back(RuntimeValue::Map{
                    {"left", link.left},
                    {"right", link.right},
                    {"leftOutPin", RuntimeValue(std::int64_t{link.leftOutPin})},
                    {"rightInPin", RuntimeValue(std::int64_t{link.rightInPin})},
                    {"linkType", RuntimeValue(link.linkType)},
                });
            }
        }
        nodeGraph.emplace(
            key, RuntimeValue(RuntimeValue::Map{
                     {"nodes", RuntimeValue(std::move(serialisedNodes))},
                     {"links", RuntimeValue(std::move(serialisedLinks))},
                 }));
    }
    result.emplace("nodeGraph", RuntimeValue(std::move(nodeGraph)));
    RuntimeValue::Map serialisedStartNodes;
    for (const auto& [key, value] : startNodes) {
        serialisedStartNodes.emplace(
            key, RuntimeValue(static_cast<std::int64_t>(value)));
    }
    result.emplace("startNodes", RuntimeValue(std::move(serialisedStartNodes)));
    return result;
}

const Graph::PinNexts& Graph::getNodeNexts(const std::string& key,
                                           int nodeIndex) const {
    const_cast<Graph*>(this)->ensureEventInitialised(key);
    static const PinNexts empty;
    const NextMap& nexts = nodeNexts();
    const auto event = nexts.find(key);
    if (event == nexts.end()) {
        return empty;
    }
    const auto node = event->second.find(nodeIndex);
    return node == event->second.end() ? empty : node->second;
}

const RuntimeIdentityPtr& Graph::getLocalGraph() const {
    return localGraph;
}

RuntimeValue Graph::getParent() const {
    if (graphContext_.isNil()) {
        return RuntimeValue();
    }
    return nodeGraphRuntime().getContextParent(graphContext_);
}

void Graph::setParent(RuntimeValue value) {
    if (graphContext_.isNil()) {
        initializeContext(std::move(value));
        return;
    }
    nodeGraphRuntime().setContextParent(graphContext_, value);
}

void Graph::setLocalGraph(RuntimeIdentityPtr context) {
    localGraph = std::move(context);
}

const RuntimeValue& Graph::getGraphContext() const {
    return graphContext_;
}

const std::string& Graph::getDoingPartKey() const {
    return doingPartKey_;
}

const Graph::DataNodeMap& Graph::dataNodes() const {
    return definition_ == nullptr ? dataNodes_ : definition_->dataNodes_;
}

const Graph::LinkMap& Graph::links() const {
    return definition_ == nullptr ? links_ : definition_->links_;
}

const Graph::EventParams& Graph::eventParams() const {
    return definition_ == nullptr ? eventParams_ : definition_->eventParams_;
}

const Graph::RelyMap& Graph::nodeRely() const {
    return definition_ == nullptr ? nodeRely_ : definition_->nodeRely_;
}

const Graph::NextMap& Graph::nodeNexts() const {
    return definition_ == nullptr ? nodeNexts_ : definition_->nodeNexts_;
}

RuntimeValue Graph::contextValue(const std::string& name) const {
    if (localGraph == nullptr) {
        return RuntimeValue();
    }
    return nodeGraphRuntime().getContextValue(localGraph, name);
}
