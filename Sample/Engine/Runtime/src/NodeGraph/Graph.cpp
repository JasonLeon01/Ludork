#include <Runtime/NodeGraph/Graph.hpp>
#include "Graph/DefinitionRuntime.hpp"
#include "Graph/ExecutionRuntime.hpp"
#include "Graph/ExecutionState.hpp"
#include "Graph/Internal.hpp"
#include "Graph/LatentRuntime.hpp"
#include "Graph/LoopRuntime.hpp"
#include "Graph/RelationRuntime.hpp"

#include <Runtime/NodeGraph/LatentManager.hpp>
#include <Runtime/NodeGraph/NodeGraphRuntime.hpp>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

using namespace ludork::runtime::graph_detail;

Graph::Graph(std::string className, RuntimeValue classValue,
             RuntimeValue parentValue, DataNodeMap inNodes, LinkMap graphLinks,
             RuntimeValue nodeModel, RuntimeValue startNodeValues,
             EventParams eventParams)
    : parentClassName(std::move(className)),
      parentClass(std::move(classValue)),
      dataNodes_(std::move(inNodes)),
      links_(std::move(graphLinks)),
      nodeModel_(std::move(nodeModel)),
      eventParams_(std::move(eventParams)),
      executionState_(std::make_unique<ExecutionState>()) {
    initializeContext(std::move(parentValue));
    startNodes =
        ludork::runtime::graph_detail::parseStartNodes(startNodeValues);
    genRelationsFromLinks();
}

Graph::~Graph() = default;

Graph::Graph(const Graph& other)
    : RuntimeObject(other),
      parentClassName(other.parentClassName),
      parentClass(other.parentClass),
      localGraph(other.localGraph),
      startNodes(other.startNodes),
      dataNodes_(other.dataNodes_),
      nodes_(other.nodes_),
      definition_(other.definition_),
      links_(other.links_),
      nodeModel_(other.nodeModel_),
      eventParams_(other.eventParams_),
      graphContext_(other.graphContext_),
      nodeRely_(other.nodeRely_),
      nodeNexts_(other.nodeNexts_),
      executionState_(std::make_unique<ExecutionState>(*other.executionState_)),
      initialised_(other.initialised_),
      initialising_(other.initialising_),
      initialisedEvents_(other.initialisedEvents_) {}

Graph& Graph::operator=(const Graph& other) {
    if (this == &other) {
        return *this;
    }
    RuntimeObject::operator=(other);
    parentClassName = other.parentClassName;
    parentClass = other.parentClass;
    localGraph = other.localGraph;
    startNodes = other.startNodes;
    dataNodes_ = other.dataNodes_;
    nodes_ = other.nodes_;
    definition_ = other.definition_;
    links_ = other.links_;
    nodeModel_ = other.nodeModel_;
    eventParams_ = other.eventParams_;
    graphContext_ = other.graphContext_;
    nodeRely_ = other.nodeRely_;
    nodeNexts_ = other.nodeNexts_;
    executionState_ = std::make_unique<ExecutionState>(*other.executionState_);
    initialised_ = other.initialised_;
    initialising_ = other.initialising_;
    initialisedEvents_ = other.initialisedEvents_;
    return *this;
}

Graph::Graph(const Graph& definition, RuntimeValue parentValue, InstanceTag)
    : parentClassName(definition.parentClassName),
      parentClass(definition.parentClass),
      startNodes(definition.startNodes),
      executionState_(std::make_unique<ExecutionState>()) {
    initializeContext(std::move(parentValue));
}

void Graph::initializeContext(RuntimeValue parentValue) {
    NodeGraphRuntimeContext context =
        nodeGraphRuntime().createContext(parentClass, parentValue);
    localGraph = std::move(context.localGraph);
    graphContext_ = std::move(context.graph);
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
        const std::vector<std::shared_ptr<DataNode>>& dataNodes =
            dataNodes_.at(key);
        for (std::size_t index = 0; index < dataNodes.size(); ++index) {
            const std::shared_ptr<DataNode>& dataNode = dataNodes[index];
            const std::shared_ptr<Node>& fallback = eventNodes[index];
            std::shared_ptr<Node> node = nodeGraphRuntime().createNode(
                nodeModel_, self, getParent(), dataNode->nodeFunction,
                fallback->getCallable(), dataNode->getParams());
            if (node == nullptr) {
                continue;
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
        ludork::runtime::graph_detail::requireCompiledDataNode(dataNode);
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
    buildRelations(links_, nodeRely_, nodeNexts_);
}

std::vector<NodeIndex> Graph::getRelyNodeIndexList(
    const std::string& key, const NodeIndex& nodeIndex) const {
    const_cast<Graph*>(this)->ensureEventInitialised(key);
    const RelyMap& relies = nodeRely();
    return dependencyOrder(relies, key, nodeIndex);
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
    return ludork::runtime::graph_detail::tryLockExecution(*executionState_,
                                                           key);
}

bool Graph::isExecutionLocked(const std::string& key) const {
    return ludork::runtime::graph_detail::isExecutionLocked(*executionState_,
                                                            key);
}

void Graph::onLatentAdded(const std::string& key) {
    ludork::runtime::graph_detail::addLatent(*executionState_, key);
}

void Graph::onLatentResolved(const std::string& key) {
    ludork::runtime::graph_detail::resolveLatent(*executionState_, key);
}

std::size_t Graph::getLatentPendingCount(const std::string& key) const {
    return ludork::runtime::graph_detail::latentCount(*executionState_, key);
}

void Graph::addExecutionCompleteCallback(const std::string& key,
                                         std::function<void()> callback) {
    ludork::runtime::graph_detail::addCompletionCallback(*executionState_, key,
                                                         std::move(callback));
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
    return executionState_->doingPartKey;
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
    executionState_->suspendedByLatent = false;
    executionState_->doingPartKey = key;
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
            executionState_->suspendedByLatent = true;
            return result;
        }

        if (metadata.loop || !metadata.loopNode.empty()) {
            ludork::runtime::graph_detail::LoopResult loop =
                executeLoopNode(key, current, result, cache, limit);
            result = std::move(loop.result);
            steps += loop.steps;
            if (steps >= limit) {
                throw std::runtime_error(
                    "Max steps exceeded while executing graph '" + key + "'");
            }
            if (executionState_->suspendedByLatent) {
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

ludork::runtime::graph_detail::LoopResult Graph::executeLoopNode(
    const std::string& key, int nodeIndex, const NodeResult& controlResult,
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

    ludork::runtime::graph_detail::LoopResult loop;
    loop.result = getLoopEmptyResult(metadata);
    if (body != nullptr) {
        const int* bodyStart = std::get_if<int>(&body->node);
        if (bodyStart != nullptr) {
            const std::vector<NodeIndex> cacheKeys =
                getLoopBodyCacheKeys(key, nodeIndex, *bodyStart);
            std::vector<NodeResult> iterations =
                iterateLoopResults(metadata, controlResult);
            for (std::size_t index = 0; index < iterations.size(); ++index) {
                const std::size_t frameBaseIndex =
                    executionState_->loopFrames.size();
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
                    std::shared_ptr<ludork::runtime::graph_detail::LoopFrame>
                        frame = std::make_shared<
                            ludork::runtime::graph_detail::LoopFrame>();
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
                    executionState_->loopFrames.insert(
                        executionState_->loopFrames.begin() +
                            static_cast<std::ptrdiff_t>(frameBaseIndex),
                        std::move(frame));
                    executionState_->suspendedByLatent = true;
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
    executionState_->suspendedByLatent = false;
    StringRestore partKeyRestore(executionState_->doingPartKey);
    executeResult(key, bodyStart, limit, &iterationCache);
    return executionState_->suspendedByLatent;
}

std::optional<int> Graph::getNamedExecPinIndex(
    const NodeMemberMetadata& metadata, const std::string& pinName) const {
    return namedExecPinIndex(metadata, pinName);
}

NodeResult Graph::getLoopEmptyResult(const NodeMemberMetadata& metadata) const {
    return emptyLoopResult(metadata);
}

std::vector<NodeResult> Graph::iterateLoopResults(
    const NodeMemberMetadata& metadata, const NodeResult& controlResult) const {
    return loopResults(metadata, controlResult);
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
    while (!executionState_->loopFrames.empty()) {
        const std::shared_ptr<ludork::runtime::graph_detail::LoopFrame> frame =
            executionState_->loopFrames.back();
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

        if (executionState_->loopFrames.back() != frame) {
            return;
        }
        executionState_->loopFrames.pop_back();
        frame->baseCache[NodeIndex(frame->loopNodeIndex)] = frame->lastResult;
        if (!frame->completedNext.has_value()) {
            continue;
        }
        executionState_->suspendedByLatent = false;
        StringRestore partKeyRestore(executionState_->doingPartKey);
        executeResult(key, *frame->completedNext, frame->limit,
                      &frame->baseCache);
        if (executionState_->suspendedByLatent) {
            return;
        }
    }
}

void Graph::completeExecution(const std::string& key) {
    const std::vector<std::function<void()>> pending =
        ludork::runtime::graph_detail::completeExecution(*executionState_, key);
    for (const std::function<void()>& callback : pending) {
        callback();
    }
}
