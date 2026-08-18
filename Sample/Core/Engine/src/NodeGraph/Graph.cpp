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

namespace ludork::engine::graph_detail {

RuntimeIdentityPtr identityValue(const RuntimeValue* value) {
    if (value == nullptr) {
        return nullptr;
    }
    const RuntimeIdentityPtr* identity = value->getIf<RuntimeIdentityPtr>();
    return identity == nullptr ? nullptr : *identity;
}

std::optional<int> integerValue(const RuntimeValue& value) {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        if (*integer < std::numeric_limits<int>::min() ||
            *integer > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }
        return static_cast<int>(*integer);
    }
    if (const double* number = value.getIf<double>()) {
        if (!std::isfinite(*number) || std::floor(*number) != *number ||
            *number < std::numeric_limits<int>::min() ||
            *number > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }
        return static_cast<int>(*number);
    }
    return std::nullopt;
}

std::optional<NodeIndex> nodeIndexValue(const RuntimeValue& value) {
    if (const std::optional<int> index = integerValue(value)) {
        return NodeIndex(*index);
    }
    if (const std::string* name = value.getIf<std::string>()) {
        return NodeIndex(*name);
    }
    return std::nullopt;
}

}  // namespace ludork::engine::graph_detail

using namespace ludork::engine::graph_detail;

Graph::Graph(std::string className, RuntimeValue classValue,
             RuntimeValue parentValue, DataNodeMap inNodes, LinkMap graphLinks,
             RuntimeValue nodeModel, RuntimeValue startNodeValues,
             EventParams eventParams)
    : parentClassName(std::move(className)),
      parentClass(std::move(classValue)),
      dataNodes_(std::move(inNodes)),
      links_(std::move(graphLinks)),
      nodeModel_(std::move(nodeModel)),
      eventParams_(std::move(eventParams)) {
    initializeContext(std::move(parentValue));
    const RuntimeValue::Map* startNodeMap =
        startNodeValues.isNil() ? nullptr
                                : startNodeValues.getIf<RuntimeValue::Map>();
    if (!startNodeValues.isNil() && startNodeMap == nullptr) {
        throw std::invalid_argument("Graph startNodes must be a map");
    }
    if (startNodeMap != nullptr) {
        for (const auto& [key, value] : *startNodeMap) {
            if (value.isNil()) {
                continue;
            }
            const std::optional<int> index = integerValue(value);
            if (!index.has_value()) {
                throw std::runtime_error("Start node for key '" + key +
                                         "' must be an integer");
            }
            startNodes.emplace(key, *index);
        }
    }
    genRelationsFromLinks();
}

Graph::Graph(const Graph& definition, RuntimeValue parentValue, InstanceTag)
    : parentClassName(definition.parentClassName),
      parentClass(definition.parentClass),
      startNodes(definition.startNodes) {
    initializeContext(std::move(parentValue));
}

void Graph::initializeContext(RuntimeValue parentValue) {
    const std::vector<RuntimeValue> context = resolveRuntime(
        "nodegraph.context", {parentClass, std::move(parentValue)});
    if (!context.empty()) {
        localGraph = identityValue(&context.front());
    }
    if (context.size() > 1) {
        graphContext_ = context[1];
    }
}

std::shared_ptr<Graph> Graph::instantiate(RuntimeValue parentValue) {
    std::shared_ptr<Graph> instance(
        new Graph(*this, std::move(parentValue), InstanceTag{}));
    instance->definition_ =
        definition_ == nullptr
            ? std::dynamic_pointer_cast<Graph>(shared_from_this())
            : definition_;
    return instance;
}

void Graph::cloneEventNodesFrom(const Graph& definition, const std::string& key,
                                const std::shared_ptr<Graph>& self) {
    const auto definitions = definition.nodes_.find(key);
    if (definitions == definition.nodes_.end()) {
        return;
    }
    std::vector<std::shared_ptr<Node>>& nodes = nodes_[key];
    nodes.clear();
    nodes.reserve(definitions->second.size());
    for (const std::shared_ptr<Node>& definitionNode : definitions->second) {
        if (definitionNode == nullptr) {
            throw std::runtime_error("Graph definition contains a nil Node");
        }
        std::shared_ptr<Node> node(
            new Node(*this, getParent(), definitionNode));
        node->attachParentGraph(self);
        nodes.push_back(std::move(node));
    }
}

void Graph::ensureInitialised() {
    if (initialised_) {
        return;
    }
    for (const auto& [key, _] : dataNodes()) {
        ensureEventInitialised(key);
    }
    initialised_ = true;
}

void Graph::ensureEventInitialised(const std::string& key) {
    if (initialisedEvents_.find(key) != initialisedEvents_.end()) {
        return;
    }
    if (initialising_) {
        return;
    }
    if (dataNodes().find(key) == dataNodes().end()) {
        throw std::runtime_error("Graph key '" + key + "' not found");
    }

    initialising_ = true;
    struct InitialisingReset {
        bool& value;
        ~InitialisingReset() {
            value = false;
        }
    } reset{initialising_};

    const std::shared_ptr<Graph> self =
        std::dynamic_pointer_cast<Graph>(weak_from_this().lock());
    if (self == nullptr) {
        throw std::logic_error("Graph owner is not shared");
    }
    if (definition_ != nullptr) {
        definition_->ensureEventInitialised(key);
        cloneEventNodesFrom(*definition_, key, self);
        initialisedEvents_.insert(key);
        initialised_ = initialisedEvents_.size() == dataNodes().size();
        return;
    }

    buildNodesForEvent(key);
    std::vector<std::shared_ptr<Node>>& eventNodes = nodes_.at(key);
    for (const std::shared_ptr<Node>& node : eventNodes) {
        node->attachParentGraph(self);
    }
    if (!nodeModel_.isNil()) {
        const RuntimeValue::Object graphObject = self;
        const std::vector<std::shared_ptr<DataNode>>& dataNodes =
            dataNodes_.at(key);
        for (std::size_t index = 0; index < dataNodes.size(); ++index) {
            const std::shared_ptr<DataNode>& dataNode = dataNodes[index];
            const std::shared_ptr<Node>& fallback = eventNodes[index];
            const std::vector<RuntimeValue> resolved = resolveRuntime(
                "nodegraph.createNode",
                {nodeModel_, RuntimeValue(graphObject), getParent(),
                 RuntimeValue(dataNode->nodeFunction),
                 RuntimeValue(fallback->getCallable()),
                 RuntimeValue(dataNode->getParams())});
            if (resolved.empty() || resolved.front().isNil()) {
                continue;
            }
            const RuntimeValue::Object* object =
                resolved.front().getIf<RuntimeValue::Object>();
            if (object == nullptr || *object == nullptr) {
                throw std::runtime_error(
                    "nodegraph.createNode must return an Engine.Node or nil");
            }
            std::shared_ptr<Node> node =
                std::dynamic_pointer_cast<Node>(*object);
            if (node == nullptr) {
                throw std::runtime_error(
                    "nodegraph.createNode returned an object that is not an "
                    "Engine.Node");
            }
            node->position = dataNode->position;
            eventNodes[index] = std::move(node);
        }
    }
    initialisedEvents_.insert(key);
    initialised_ = initialisedEvents_.size() == dataNodes().size();
}

void Graph::genNodesFromDataNodes() {
    initialised_ = false;
    nodes_.clear();
    initialisedEvents_.clear();
    ensureInitialised();
}

void Graph::buildNodesFromDataNodes() {
    nodes_.clear();
    initialisedEvents_.clear();
    for (const auto& [key, _] : dataNodes_) {
        buildNodesForEvent(key);
    }
}

void Graph::buildNodesForEvent(const std::string& key) {
    const auto event = dataNodes_.find(key);
    if (event == dataNodes_.end()) {
        return;
    }
    std::vector<std::shared_ptr<Node>>& nodes = nodes_[key];
    nodes.clear();
    nodes.reserve(event->second.size());
    for (const std::shared_ptr<DataNode>& dataNode : event->second) {
        if (dataNode == nullptr) {
            throw std::runtime_error("Graph contains a nil DataNode");
        }
        if (dataNode->getResolvedDefinition().isNil()) {
            throw std::runtime_error("DataNode '" + dataNode->nodeFunction +
                                     "' is not compiled");
        }
        std::shared_ptr<Node> node(
            new Node(*this, getParent(), dataNode->nodeFunction,
                     dataNode->getResolvedDefinition(),
                     RuntimeValue(dataNode->getParams())));
        node->position = dataNode->position;
        if (node->getCallable() == nullptr) {
            throw std::runtime_error("Function " + dataNode->nodeFunction +
                                     " not found");
        }
        nodes.push_back(std::move(node));
    }
}

void Graph::genRelationsFromLinks() {
    if (definition_ != nullptr) {
        return;
    }
    nodeRely_.clear();
    nodeNexts_.clear();
    for (const auto& [key, links] : links_) {
        EventRely& eventRely = nodeRely_[key];
        EventNexts& eventNexts = nodeNexts_[key];
        for (const GraphLink& link : links) {
            const std::optional<NodeIndex> left = nodeIndexValue(link.left);
            const std::optional<NodeIndex> right = nodeIndexValue(link.right);
            if (!left.has_value() || !right.has_value()) {
                continue;
            }
            if (link.linkType == "Params") {
                const int* rightIndex = std::get_if<int>(&*right);
                if (rightIndex != nullptr) {
                    eventRely[*rightIndex][link.rightInPin] =
                        NodeSource{*left, link.leftOutPin};
                }
            } else if (link.linkType == "Exec") {
                const int* leftIndex = std::get_if<int>(&*left);
                if (leftIndex != nullptr) {
                    eventNexts[*leftIndex][link.leftOutPin] =
                        NodeSource{*right, link.rightInPin};
                }
            }
        }
    }
}
