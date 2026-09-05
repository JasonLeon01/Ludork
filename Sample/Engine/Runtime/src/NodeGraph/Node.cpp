#include <Runtime/NodeGraph/Node.hpp>

#include <Runtime/NodeGraph/Graph.hpp>
#include <Runtime/NodeGraph/NodeGraphRuntime.hpp>
#include <Runtime/TypedDataService.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

const RuntimeValue* mapValue(const RuntimeValue::Map& map,
                             const std::string& name) {
    const auto iterator = map.find(name);
    return iterator == map.end() ? nullptr : &iterator->second;
}

const RuntimeValue::Map* asMap(const RuntimeValue* value) {
    return value == nullptr ? nullptr : value->getIf<RuntimeValue::Map>();
}

const RuntimeValue::Array* asArray(const RuntimeValue* value) {
    return value == nullptr ? nullptr : value->getIf<RuntimeValue::Array>();
}

std::string stringValue(const RuntimeValue* value,
                        const std::string& fallback = std::string()) {
    if (value == nullptr) {
        return fallback;
    }
    const std::string* text = value->getIf<std::string>();
    return text == nullptr ? fallback : *text;
}

bool boolValue(const RuntimeValue* value, bool fallback = false) {
    if (value == nullptr) {
        return fallback;
    }
    const bool* flag = value->getIf<bool>();
    return flag == nullptr ? fallback : *flag;
}

RuntimeIdentityPtr identityValue(const RuntimeValue* value) {
    if (value == nullptr) {
        return nullptr;
    }
    const RuntimeHandle* identity = value->getIf<RuntimeHandle>();
    return identity == nullptr ? nullptr : identity->identity();
}

RuntimeValue::Array singleOrArray(const RuntimeValue* value) {
    if (value == nullptr) {
        return {};
    }
    if (const RuntimeValue::Array* array =
            value->getIf<RuntimeValue::Array>()) {
        return *array;
    }
    return {*value};
}

std::vector<NodeNamedValues> parseNamedValues(const RuntimeValue* value) {
    std::vector<NodeNamedValues> result;
    const RuntimeValue::Array* entries = asArray(value);
    if (entries == nullptr) {
        return result;
    }
    result.reserve(entries->size());
    for (const RuntimeValue& entryValue : *entries) {
        const RuntimeValue::Map* entry = entryValue.getIf<RuntimeValue::Map>();
        if (entry == nullptr) {
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
        const RuntimeValue::Array* array = values.getIf<RuntimeValue::Array>();
        const RuntimeValue::Map* map = values.getIf<RuntimeValue::Map>();
        if (array == nullptr && (map == nullptr || !map->empty())) {
            throw std::invalid_argument("DataNode params must be an array");
        }
        if (array != nullptr) {
            resolvedParams = *array;
        }
    }
    params = RuntimeValue(std::move(resolvedParams));
}

RuntimeValue::Array DataNode::getParams() const {
    if (params.isNil()) {
        return {};
    }
    const RuntimeValue::Array* values = params.getIf<RuntimeValue::Array>();
    const RuntimeValue::Map* map = params.getIf<RuntimeValue::Map>();
    if (values == nullptr && (map == nullptr || !map->empty())) {
        throw std::invalid_argument("DataNode params must be an array");
    }
    return values == nullptr ? RuntimeValue::Array{} : *values;
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
        const RuntimeValue::Array* array = values.getIf<RuntimeValue::Array>();
        if (array == nullptr) {
            throw std::invalid_argument("Node params must be an array");
        }
        rawParams = *array;
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
    const Node& definition = compiledDefinition();
    const RuntimeValue parent = getParent();
    const RuntimeValue::Array* storedParams =
        params.getIf<RuntimeValue::Array>();
    if (storedParams == nullptr) {
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
                                        ? (*storedParams)[index]
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
        nodeGraphRuntime().setContextValue(
            context, "__key__", RuntimeValue(parentGraph->getDoingPartKey()));
    }
    NodeResult result = nodeGraphRuntime().invoke(
        definition.nodeFunction_, selfValue, actualParams, context);
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
    return nodeGraphRuntime().refLocal(nodeFunction);
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
    const RuntimeValue::Map* descriptor =
        resolvedDefinition.getIf<RuntimeValue::Map>();
    if (descriptor == nullptr) {
        throw std::runtime_error("Node resolved definition must be a map");
    }
    result.descriptor = *descriptor;
    result.callable = identityValue(mapValue(*descriptor, "callable"));
    result.selfFunction = boolValue(mapValue(*descriptor, "isSelf"));
    result.declaringModule =
        stringValue(mapValue(*descriptor, "declaringModule"));
    result.displayName = stringValue(mapValue(*descriptor, "displayName"));
    if (const RuntimeValue::Array* names =
            asArray(mapValue(*descriptor, "paramNames"))) {
        for (const RuntimeValue& value : *names) {
            if (const std::string* parameterName = value.getIf<std::string>()) {
                result.parameterNames.push_back(*parameterName);
            }
        }
    }
    const RuntimeValue* metadata = mapValue(*descriptor, "memberMeta");
    result.metadata =
        parseMemberMetadata(metadata == nullptr ? RuntimeValue() : *metadata);
    return result;
}

NodeMemberMetadata Node::parseMemberMetadata(
    const RuntimeValue& metadataValue) {
    NodeMemberMetadata metadata;
    const RuntimeValue::Map* value = metadataValue.getIf<RuntimeValue::Map>();
    if (value == nullptr) {
        return metadata;
    }

    const RuntimeValue::Map* parameterTypes =
        asMap(mapValue(*value, "parameterTypes"));
    const RuntimeValue::Array* parameters =
        asArray(mapValue(*value, "parameters"));
    if (parameters != nullptr) {
        for (const RuntimeValue& parameterValue : *parameters) {
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
                if (parameterTypes != nullptr) {
                    const auto type = parameterTypes->find(*name);
                    if (type != parameterTypes->end()) {
                        parameterType = type->second;
                    }
                }
                metadata.parameterTypes.emplace(*name,
                                                std::move(parameterType));
                continue;
            }
            const RuntimeValue::Map* entry =
                parameterValue.getIf<RuntimeValue::Map>();
            if (entry == nullptr) {
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
            const RuntimeValue* type = mapValue(*entry, "type");
            metadata.parameterTypes.emplace(
                name,
                type == nullptr ? RuntimeValue(std::string("any")) : *type);
        }
    }

    const RuntimeValue::Array* defaults = asArray(mapValue(*value, "defaults"));
    if (defaults == nullptr) {
        defaults = asArray(mapValue(*value, "default"));
    }
    if (defaults != nullptr) {
        const std::size_t count =
            std::min(defaults->size(), metadata.parameterOrder.size());
        for (std::size_t index = 0; index < count; ++index) {
            if (!(*defaults)[index].isNil()) {
                metadata.parameterDefaults.emplace(
                    metadata.parameterOrder[index], (*defaults)[index]);
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
