#include <Runtime/NodeGraph/Node.hpp>

#include <Runtime/NodeGraph/Graph.hpp>
#include "NodeGraphRuntime/NodeGraphRuntimeInternal.hpp"
#include <Runtime/TypedDataService.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

std::optional<RuntimeValueView> mapValue(RuntimeMapView map,
                                         const std::string& name) {
    return map.find(name);
}

std::optional<RuntimeMapView> asMap(
    const std::optional<RuntimeValueView>& value) {
    return !value ? std::nullopt : RuntimeValueView(*value).map();
}

std::optional<RuntimeArrayView> asArray(
    const std::optional<RuntimeValueView>& value) {
    return !value ? std::nullopt : RuntimeValueView(*value).array();
}

std::string stringValue(const std::optional<RuntimeValueView>& value,
                        const std::string& fallback = std::string()) {
    if (!value) {
        return fallback;
    }
    const std::string* text = value->getIf<std::string>();
    return text == nullptr ? fallback : *text;
}

bool boolValue(const std::optional<RuntimeValueView>& value,
               bool fallback = false) {
    if (!value) {
        return fallback;
    }
    const bool* flag = value->getIf<bool>();
    return flag == nullptr ? fallback : *flag;
}

RuntimeIdentityPtr identityValue(const std::optional<RuntimeValueView>& value) {
    if (!value) {
        return nullptr;
    }
    const RuntimeHandle* identity = value->getIf<RuntimeHandle>();
    return identity == nullptr ? nullptr : identity->identity();
}

RuntimeValue::Array singleOrArray(
    const std::optional<RuntimeValueView>& value) {
    if (!value) {
        return {};
    }
    if (std::optional<RuntimeArrayView> array =
            RuntimeValueView(*value).array()) {
        return array->toArray();
    }
    return {value->toValue()};
}

std::vector<NodeNamedValues> parseNamedValues(
    const std::optional<RuntimeValueView>& value) {
    std::vector<NodeNamedValues> result;
    std::optional<RuntimeArrayView> entries = asArray(value);
    if (!entries) {
        return result;
    }
    result.reserve(entries->size());
    for (RuntimeValueView entryValue : *entries) {
        std::optional<RuntimeMapView> entry =
            RuntimeValueView(entryValue).map();
        if (!entry) {
            continue;
        }
        const std::string name = stringValue(mapValue(*entry, "name"));
        if (name.empty()) {
            throw std::runtime_error("Node metadata pin is missing its name");
        }
        result.push_back(
            NodeNamedValues{name, singleOrArray(mapValue(*entry, "values"))});
    }
    return result;
}

}  // namespace

DataNode::DataNode(std::string function, RuntimeValue values,
                   RuntimeValue resolvedDefinition)
    : nodeFunction(std::move(function)),
      resolvedDefinition_(std::move(resolvedDefinition)) {
    RuntimeValue::Array resolvedParams;
    if (!values.isNil()) {
        std::optional<RuntimeArrayView> array =
            RuntimeValueView(values).array();
        std::optional<RuntimeMapView> map = RuntimeValueView(values).map();
        if (!array && (!map || !map->empty())) {
            throw std::invalid_argument("DataNode params must be an array");
        }
        if (array.has_value()) {
            resolvedParams = array->toArray();
        }
    }
    params = RuntimeValue(std::move(resolvedParams));
}

RuntimeValue::Array DataNode::getParams() const {
    if (params.isNil()) {
        return {};
    }
    std::optional<RuntimeArrayView> values = RuntimeValueView(params).array();
    std::optional<RuntimeMapView> map = RuntimeValueView(params).map();
    if (!values && (!map || !map->empty())) {
        throw std::invalid_argument("DataNode params must be an array");
    }
    return !values ? RuntimeValue::Array{} : values->toArray();
}

const RuntimeValue& DataNode::getResolvedDefinition() const {
    return resolvedDefinition_;
}

Node::Node(std::shared_ptr<Graph> graph, RuntimeValue parentValue,
           std::string name, RuntimeIdentityPtr function, RuntimeValue values)
    : functionName(std::move(name)),
      parentGraph_(graph),
      nodeFunction_(std::move(function)) {
    static_cast<void>(parentValue);
    if (graph == nullptr) {
        throw std::invalid_argument("Node parent graph must not be null");
    }
    RuntimeValue::Map resolvedDefinition{
        {"callable", RuntimeValue(nodeFunction_)},
    };
    initialise(std::move(values), RuntimeValue(std::move(resolvedDefinition)));
}

Node::Node(Graph& graph, RuntimeValue parentValue, std::string name,
           RuntimeValue resolvedDefinition, RuntimeValue values)
    : functionName(std::move(name)), nodeFunction_() {
    static_cast<void>(graph);
    static_cast<void>(parentValue);
    initialise(std::move(values), std::move(resolvedDefinition));
}

Node::Node(Graph& graph, RuntimeValue parentValue,
           std::shared_ptr<const Node> definition)
    : functionName(definition->functionName),
      params(definition->params),
      position(definition->position),
      definition_(std::move(definition)) {
    static_cast<void>(graph);
    static_cast<void>(parentValue);
}

void Node::initialise(RuntimeValue values, RuntimeValue resolvedDefinition) {
    analyseFunction(resolvedDefinition);
    RuntimeValue::Array rawParams;
    if (!values.isNil()) {
        std::optional<RuntimeArrayView> array =
            RuntimeValueView(values).array();
        if (!array) {
            throw std::invalid_argument("Node params must be an array");
        }
        rawParams = array->toArray();
    }
    paramCount_ = rawParams.size();
    params = RuntimeValue(resolveStoredParams(rawParams));
}

void Node::attachParentGraph(const std::shared_ptr<Graph>& parentGraph) {
    parentGraph_ = parentGraph;
}

const Node& Node::compiledDefinition() const {
    return definition_ == nullptr ? *this : *definition_;
}

RuntimeValue::Map Node::getParamList() const {
    return compiledDefinition().paramList_;
}

RuntimeValue::Map Node::getParamDefaults() const {
    return compiledDefinition().paramDefaults_;
}

RuntimeValue::Array Node::execute(const InputPinMap& inputPinReplace) {
    return executeResult(inputPinReplace).values;
}

NodeResult Node::executeResult(const InputPinMap& inputPinReplace) {
    ludork::runtime::RuntimeScope scope;
    const Node& definition = compiledDefinition();
    const RuntimeValue parent = getParent();
    std::optional<RuntimeArrayView> storedParams =
        RuntimeValueView(params).array();
    if (!storedParams) {
        throw std::invalid_argument("Node params must be an array");
    }
    RuntimeValue::Array actualParams(definition.paramCount_);

    for (std::size_t index = 0; index < definition.paramCount_; ++index) {
        const auto replacement = inputPinReplace.find(static_cast<int>(index));
        if (replacement != inputPinReplace.end()) {
            actualParams[index] = replacement->second;
            continue;
        }

        const RuntimeValue stored = index < storedParams->size()
                                        ? (*storedParams)[index].toValue()
                                        : RuntimeValue();
        if (const std::string* text = stored.getIf<std::string>();
            text != nullptr && *text == "self") {
            actualParams[index] = parent;
            continue;
        }
        actualParams[index] = stored;
    }

    if (definition.nodeFunction_ == nullptr) {
        throw std::runtime_error("Node function '" + functionName +
                                 "' is not callable");
    }

    const RuntimeValue selfValue =
        definition.selfFunction_ ? parent : RuntimeValue();
    const std::shared_ptr<Graph> parentGraph = parentGraph_.lock();
    const RuntimeIdentityPtr context =
        parentGraph == nullptr ? nullptr : parentGraph->getLocalGraph();
    if (context != nullptr) {
        ludork::runtime::node_graph_detail::setNodeGraphContextValue(
            scope, RuntimeHandle(context), "__key__",
            RuntimeValue(parentGraph->getDoingPartKey()));
    }
    NodeResult result =
        ludork::runtime::node_graph_detail::invokeNodeGraphCallable(
            scope, RuntimeHandle(definition.nodeFunction_), selfValue,
            actualParams, RuntimeHandle(context));
    if (result.count == 0) {
        result.values = {RuntimeValue()};
        result.count = 1;
    } else if (result.values.size() < result.count) {
        result.values.resize(result.count);
    }
    return result;
}

RuntimeValue Node::getParent() const {
    const std::shared_ptr<Graph> graph = parentGraph_.lock();
    return graph == nullptr ? RuntimeValue() : graph->getParent();
}

RuntimeValue::Map Node::asDict() const {
    RuntimeValue::Map result{
        {"nodeFunction", RuntimeValue(functionName)},
        {"params", params},
    };
    if (!position.isNil()) {
        result.emplace("pos", position);
    }
    return result;
}

std::string Node::repr() const {
    const Node& definition = compiledDefinition();
    std::ostringstream stream;
    stream << definition.funcInfo_ << '(' << definition.paramCount_
           << " params)";
    return stream.str();
}

std::string Node::toString() const {
    return repr();
}

RuntimeIdentityPtr Node::getRefLocal(const RuntimeIdentityPtr& nodeFunction) {
    if (nodeFunction == nullptr) {
        return nullptr;
    }
    ludork::runtime::RuntimeScope scope;
    return ludork::runtime::node_graph_detail::nodeGraphRefLocal(
               scope, RuntimeHandle(nodeFunction))
        .identity();
}

const NodeMemberMetadata& Node::getMemberMetadata() const {
    return compiledDefinition().memberMetadata_;
}

const RuntimeIdentityPtr& Node::getCallable() const {
    return compiledDefinition().nodeFunction_;
}

std::shared_ptr<Graph> Node::getParentGraph() const {
    return parentGraph_.lock();
}

Node::ResolvedCallable Node::resolvedCallable(
    const RuntimeValue& resolvedDefinition) {
    ResolvedCallable result;
    std::optional<RuntimeMapView> descriptor =
        RuntimeValueView(resolvedDefinition).map();
    if (!descriptor) {
        throw std::runtime_error("Node resolved definition must be a map");
    }
    result.descriptor = descriptor->toMap();
    result.callable = identityValue(mapValue(*descriptor, "callable"));
    result.selfFunction = boolValue(mapValue(*descriptor, "isSelf"));
    result.declaringModule =
        stringValue(mapValue(*descriptor, "declaringModule"));
    result.displayName = stringValue(mapValue(*descriptor, "displayName"));
    if (std::optional<RuntimeArrayView> names =
            asArray(mapValue(*descriptor, "paramNames"))) {
        for (RuntimeValueView value : *names) {
            if (const std::string* parameterName = value.getIf<std::string>()) {
                result.parameterNames.push_back(*parameterName);
            }
        }
    }
    const auto metadata = mapValue(*descriptor, "memberMeta");
    result.metadata =
        parseMemberMetadata(metadata.value_or(RuntimeValueView()));
    return result;
}

NodeMemberMetadata Node::parseMemberMetadata(RuntimeValueView metadataValue) {
    NodeMemberMetadata metadata;
    std::optional<RuntimeMapView> value = RuntimeValueView(metadataValue).map();
    if (!value) {
        return metadata;
    }

    std::optional<RuntimeMapView> parameterTypes =
        asMap(mapValue(*value, "parameterTypes"));
    std::optional<RuntimeArrayView> parameters =
        asArray(mapValue(*value, "parameters"));
    if (parameters.has_value()) {
        for (RuntimeValueView parameterValue : *parameters) {
            if (const std::string* name = parameterValue.getIf<std::string>()) {
                if (std::find(metadata.parameterOrder.begin(),
                              metadata.parameterOrder.end(),
                              *name) != metadata.parameterOrder.end()) {
                    throw std::runtime_error(
                        "parameters order contains duplicate key '" + *name +
                        "'");
                }
                metadata.parameterOrder.push_back(*name);
                RuntimeValue parameterType(std::string("any"));
                if (parameterTypes.has_value()) {
                    const auto type = parameterTypes->find(*name);
                    if (type.has_value()) {
                        parameterType = type->toValue();
                    }
                }
                metadata.parameterTypes.emplace(*name,
                                                std::move(parameterType));
                continue;
            }
            std::optional<RuntimeMapView> entry =
                RuntimeValueView(parameterValue).map();
            if (!entry) {
                continue;
            }
            const std::string name = stringValue(mapValue(*entry, "name"));
            if (name.empty()) {
                throw std::runtime_error(
                    "parameters order contains an unknown key");
            }
            if (metadata.parameterTypes.find(name) !=
                metadata.parameterTypes.end()) {
                throw std::runtime_error(
                    "parameters order contains duplicate key '" + name + "'");
            }
            metadata.parameterOrder.push_back(name);
            const auto type = mapValue(*entry, "type");
            metadata.parameterTypes.emplace(
                name,
                !type ? RuntimeValue(std::string("any")) : type->toValue());
        }
    }

    std::optional<RuntimeArrayView> defaults =
        asArray(mapValue(*value, "defaults"));
    if (!defaults) {
        defaults = asArray(mapValue(*value, "default"));
    }
    if (defaults.has_value()) {
        const std::size_t count =
            std::min(defaults->size(), metadata.parameterOrder.size());
        for (std::size_t index = 0; index < count; ++index) {
            if (!(*defaults)[index].isNil()) {
                metadata.parameterDefaults.emplace(
                    metadata.parameterOrder[index],
                    (*defaults)[index].toValue());
            }
        }
    }

    metadata.execSplits = parseNamedValues(mapValue(*value, "execSplit"));
    metadata.latentStates = parseNamedValues(mapValue(*value, "latentStates"));
    metadata.latent = boolValue(mapValue(*value, "latent"));
    metadata.loop = boolValue(mapValue(*value, "loop"));
    metadata.pure = boolValue(mapValue(*value, "pure"));
    metadata.loopNode = stringValue(mapValue(*value, "loopNode"));
    metadata.kind = stringValue(mapValue(*value, "kind"));
    return metadata;
}

void Node::analyseFunction(const RuntimeValue& resolvedDefinition) {
    ResolvedCallable callable = resolvedCallable(resolvedDefinition);
    if (callable.callable != nullptr) {
        nodeFunction_ = std::move(callable.callable);
    }
    memberMetadata_ = std::move(callable.metadata);
    declaringModule_ = std::move(callable.declaringModule);
    selfFunction_ = callable.selfFunction;
    funcInfo_ = std::move(callable.displayName);
    if (funcInfo_.empty()) {
        const std::size_t separator = functionName.find_last_of('.');
        funcInfo_ = separator == std::string::npos
                        ? functionName
                        : functionName.substr(separator + 1);
    }

    paramOrder_ = memberMetadata_.parameterOrder.empty()
                      ? std::move(callable.parameterNames)
                      : memberMetadata_.parameterOrder;
    for (const std::string& parameterName : paramOrder_) {
        const auto type = memberMetadata_.parameterTypes.find(parameterName);
        paramList_.emplace(parameterName,
                           type == memberMetadata_.parameterTypes.end()
                               ? RuntimeValue(std::string("any"))
                               : type->second);
    }
    paramDefaults_ = memberMetadata_.parameterDefaults;
}

RuntimeValue::Array Node::resolveStoredParams(
    const RuntimeValue::Array& rawParams) {
    RuntimeValue::Array result;
    result.reserve(rawParams.size());
    for (std::size_t index = 0; index < rawParams.size(); ++index) {
        const RuntimeValue& value = rawParams[index];
        if (const std::string* text = value.getIf<std::string>();
            text != nullptr && *text == "self") {
            result.push_back(value);
            continue;
        }
        if (value.isNil()) {
            result.emplace_back();
            continue;
        }
        RuntimeValue parameterType(std::string("any"));
        if (index < paramOrder_.size()) {
            const auto type = paramList_.find(paramOrder_[index]);
            if (type != paramList_.end()) {
                parameterType = type->second;
            }
        }
        result.push_back(typedDataService().resolveTypedDataValue(
            value, parameterType, {}, declaringModule_));
    }
    return result;
}
