#include <NodeGraph/Graph.hpp>

#include <NodeGraph/LatentManager.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace {

const RuntimeValue* mapValue(const RuntimeValue::Map& map,
                             const std::string& name) {
    const auto iterator = map.find(name);
    return iterator == map.end() ? nullptr : &iterator->second;
}

RuntimeIdentityPtr identityValue(const RuntimeValue* value) {
    if (value == nullptr) {
        return nullptr;
    }
    const RuntimeIdentityPtr* identity = value->getIf<RuntimeIdentityPtr>();
    return identity == nullptr ? nullptr : *identity;
}

std::optional<std::size_t> sizeValue(const RuntimeValue& value) {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        if (*integer < 0) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(*integer);
    }
    if (const double* number = value.getIf<double>()) {
        if (!std::isfinite(*number) || std::floor(*number) != *number ||
            *number < 0.0 ||
            *number >
                static_cast<double>(std::numeric_limits<std::size_t>::max())) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(*number);
    }
    return std::nullopt;
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

RuntimeValue nodeIndexRuntimeValue(const NodeIndex& value) {
    if (const int* index = std::get_if<int>(&value)) {
        return RuntimeValue(static_cast<std::int64_t>(*index));
    }
    return RuntimeValue(std::get<std::string>(value));
}

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

RuntimeValue runtimeMember(const RuntimeValue& value, const std::string& name) {
    const std::vector<RuntimeValue> resolved =
        resolveRuntime("reflect.get", {value, RuntimeValue(name)});
    return resolved.empty() ? RuntimeValue() : resolved.front();
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

NodeCache decodeNodeCache(const RuntimeIdentityPtr& cacheIdentity) {
    NodeCache cache;
    if (cacheIdentity == nullptr) {
        return cache;
    }
    const std::vector<RuntimeValue> resolved = resolveRuntime(
        "nodegraph.cache",
        {RuntimeValue(std::string("decode")), RuntimeValue(cacheIdentity)});
    if (resolved.empty() || resolved.front().isNil()) {
        return cache;
    }
    const RuntimeValue::Array* descriptors =
        resolved.front().getIf<RuntimeValue::Array>();
    if (descriptors == nullptr) {
        throw std::runtime_error(
            "nodegraph.cache decode must return a descriptor array");
    }
    for (const RuntimeValue& descriptorValue : *descriptors) {
        const RuntimeValue::Map* descriptor =
            descriptorValue.getIf<RuntimeValue::Map>();
        if (descriptor == nullptr) {
            throw std::runtime_error("Node cache descriptor must be a map");
        }
        const RuntimeValue* keyValue = mapValue(*descriptor, "key");
        const std::optional<NodeIndex> key =
            keyValue == nullptr ? std::nullopt : nodeIndexValue(*keyValue);
        if (!key.has_value()) {
            throw std::runtime_error(
                "Node cache descriptor key must be an integer or string");
        }
        const RuntimeValue* valuesValue = mapValue(*descriptor, "values");
        const RuntimeValue::Array* values =
            valuesValue == nullptr ? nullptr
                                   : valuesValue->getIf<RuntimeValue::Array>();
        if (valuesValue != nullptr && values == nullptr) {
            throw std::runtime_error(
                "Node cache descriptor values must be an array");
        }
        NodeResult result;
        if (values != nullptr) {
            result.values = *values;
        }
        const RuntimeValue* countValue = mapValue(*descriptor, "count");
        if (countValue == nullptr) {
            result.count = result.values.size();
        } else {
            const std::optional<std::size_t> count = sizeValue(*countValue);
            if (!count.has_value()) {
                throw std::runtime_error(
                    "Node cache descriptor count must be a non-negative "
                    "integer");
            }
            result.count = *count;
        }
        if (result.values.size() < result.count) {
            result.values.resize(result.count);
        }
        cache[*key] = std::move(result);
    }
    return cache;
}

RuntimeValue::Array encodeNodeCache(const NodeCache& cache) {
    RuntimeValue::Array descriptors;
    descriptors.reserve(cache.size());
    for (const auto& [key, result] : cache) {
        descriptors.emplace_back(RuntimeValue::Map{
            {"key", nodeIndexRuntimeValue(key)},
            {"values", RuntimeValue(result.values)},
            {"count", RuntimeValue(static_cast<std::int64_t>(result.count))},
        });
    }
    return descriptors;
}

void syncNodeCache(const RuntimeIdentityPtr& cacheIdentity,
                   const NodeCache& cache) {
    if (cacheIdentity == nullptr) {
        return;
    }
    resolveRuntime("nodegraph.cache", {RuntimeValue(std::string("encode")),
                                       RuntimeValue(cacheIdentity),
                                       RuntimeValue(encodeNodeCache(cache))});
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
    const std::vector<RuntimeValue> resolved =
        resolveRuntime("nodegraph.context",
                       {graphContext_, RuntimeValue(std::string("getParent"))});
    return resolved.empty() ? RuntimeValue() : resolved.front();
}

void Graph::setParent(RuntimeValue value) {
    if (graphContext_.isNil()) {
        initializeContext(std::move(value));
        return;
    }
    resolveRuntime("nodegraph.context",
                   {graphContext_, RuntimeValue(std::string("setParent")),
                    std::move(value)});
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
    const std::vector<RuntimeValue> resolved =
        resolveRuntime("nodegraph.context",
                       {RuntimeValue(localGraph),
                        RuntimeValue(std::string("get")), RuntimeValue(name)});
    return resolved.empty() ? RuntimeValue() : resolved.front();
}
