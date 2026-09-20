#include <Runtime/NodeGraph/Node.hpp>

#include <Runtime/NodeGraph/Graph.hpp>
#include "NodeGraphRuntime/NodeGraphRuntimeInternal.hpp"
#include <Runtime/TypedDataService.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

Node::Node(std::shared_ptr<Graph> graph, RuntimeValue parentValue,
           std::string name, RuntimeIdentityPtr function, RuntimeValue values)
    : functionName(std::move(name)),
      parentGraph_(graph),
      definition_(std::make_shared<const NodeDefinition>(std::move(function))) {
    static_cast<void>(parentValue);
    if (graph == nullptr) {
        throw std::invalid_argument("Node parent graph must not be null");
    }
    initialise(std::move(values));
}

Node::Node(Graph& graph, RuntimeValue parentValue, std::string name,
           std::shared_ptr<const NodeDefinition> definition,
           RuntimeValue values)
    : functionName(std::move(name)), definition_(std::move(definition)) {
    static_cast<void>(graph);
    static_cast<void>(parentValue);
    initialise(std::move(values));
}

Node::Node(Graph& graph, RuntimeValue parentValue,
           std::shared_ptr<const Node> definition)
    : functionName(definition->functionName),
      params(definition->params),
      position(definition->position),
      definition_(definition->definition_),
      funcInfo_(definition->funcInfo_),
      paramCount_(definition->paramCount_) {
    static_cast<void>(graph);
    static_cast<void>(parentValue);
}

void Node::initialise(RuntimeValue values) {
    funcInfo_ = definition_->displayName;
    if (funcInfo_.empty()) {
        const std::size_t separator = functionName.find_last_of('.');
        funcInfo_ = separator == std::string::npos
                        ? functionName
                        : functionName.substr(separator + 1);
    }
    RuntimeValue::Array rawParams;
    if (!values.isNil()) {
        std::optional<RuntimeArrayView> array =
            RuntimeValueView(values).array();
        if (!array) {
            throw std::invalid_argument("Node params must be an array");
        }
        rawParams = array->toArray();
    }
    paramCount_ = std::max(rawParams.size(), definition_->parameters.size());
    params = RuntimeValue(resolveStoredParams(rawParams));
}

void Node::attachParentGraph(const std::shared_ptr<Graph>& parentGraph) {
    parentGraph_ = parentGraph;
}

RuntimeValue::Map Node::getParamList() const {
    RuntimeValue::Map result;
    result.reserve(definition_->parameters.size());
    for (const NodeDefinition::CompiledParameter& parameter :
         definition_->parameters) {
        result.emplace(parameter.name, parameter.typeReference());
    }
    return result;
}

RuntimeValue::Map Node::getParamDefaults() const {
    RuntimeValue::Map result;
    for (const NodeDefinition::CompiledParameter& parameter :
         definition_->parameters) {
        if (parameter.defaultValue) {
            result.emplace(parameter.name, *parameter.defaultValue);
        }
    }
    return result;
}

RuntimeValue::Array Node::execute(const InputPinMap& inputPinReplace) {
    return executeResult(inputPinReplace).values;
}

NodeResult Node::executeResult(const InputPinMap& inputPinReplace) {
    ludork::runtime::RuntimeScope scope;
    const NodeDefinition& definition = *definition_;
    const RuntimeValue parent = getParent();
    std::optional<RuntimeArrayView> storedParams =
        RuntimeValueView(params).array();
    if (!storedParams) {
        throw std::invalid_argument("Node params must be an array");
    }
    RuntimeValue::Array actualParams(paramCount_);

    for (std::size_t index = 0; index < paramCount_; ++index) {
        const auto replacement = inputPinReplace.find(static_cast<int>(index));
        const bool connected = replacement != inputPinReplace.end();
        const bool receiver = definition.hasSelfParameter && index == 0;
        RuntimeValue value = connected ? replacement->second
                             : index < storedParams->size()
                                 ? (*storedParams)[index].toValue()
                                 : RuntimeValue();
        bool useParent = !connected && receiver && value.isNil();
        if (!connected) {
            const std::string* text = value.getIf<std::string>();
            useParent = useParent || (text != nullptr && *text == "self");
        }
        if (useParent) {
            value = parent;
        }
        if (receiver && value.isNil()) {
            throw std::runtime_error("Node " + functionName +
                                     " parameter self requires an object");
        }
        if ((connected || receiver || useParent) &&
            index < definition.parameters.size()) {
            try {
                value = typedDataService().resolveRuntimeTypedValue(
                    value, definition.parameters[index].schema(),
                    definition.declaringModule);
            } catch (const std::exception& error) {
                throw std::runtime_error(
                    "Node " + functionName + " parameter " +
                    std::to_string(index + 1) + ": " + error.what());
            }
        }
        actualParams[index] = std::move(value);
    }

    if (definition.callable == nullptr) {
        throw std::runtime_error("Node function '" + functionName +
                                 "' is not callable");
    }

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
            scope, RuntimeHandle(definition.callable), RuntimeValue(),
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
    std::ostringstream stream;
    stream << funcInfo_ << '(' << paramCount_ << " params)";
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
    return definition_->metadata;
}

const RuntimeIdentityPtr& Node::getCallable() const {
    return definition_->callable;
}

std::shared_ptr<Graph> Node::getParentGraph() const {
    return parentGraph_.lock();
}

RuntimeValue::Array Node::resolveStoredParams(
    const RuntimeValue::Array& rawParams) {
    RuntimeValue::Array result;
    result.reserve(paramCount_);
    const ludork::runtime::TypeSchema anyType;
    for (std::size_t index = 0; index < paramCount_; ++index) {
        const NodeDefinition::CompiledParameter* parameter =
            index < definition_->parameters.size()
                ? &definition_->parameters[index]
                : nullptr;
        RuntimeValue value =
            index < rawParams.size() ? rawParams[index] : RuntimeValue();
        if (value.isNil() && parameter != nullptr && parameter->defaultValue) {
            value = *parameter->defaultValue;
        }
        if (const std::string* text = value.getIf<std::string>();
            text != nullptr && *text == "self") {
            result.push_back(value);
            continue;
        }
        if (value.isNil()) {
            result.emplace_back();
            continue;
        }
        try {
            result.push_back(typedDataService().resolveTypedDataValue(
                value, parameter == nullptr ? anyType : parameter->schema(), {},
                definition_->declaringModule));
        } catch (const std::exception& error) {
            throw std::runtime_error("Node " + functionName + " parameter " +
                                     std::to_string(index + 1) + ": " +
                                     error.what());
        }
    }
    return result;
}
