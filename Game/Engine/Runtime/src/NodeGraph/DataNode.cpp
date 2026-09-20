#include <Runtime/NodeGraph/DataNode.hpp>
#include <Runtime/NodeGraph/NodeDefinition.hpp>

#include <stdexcept>
#include <utility>

DataNode::DataNode(std::string function, RuntimeValue values,
                   RuntimeValue resolvedDefinition)
    : nodeFunction(std::move(function)),
      definition_(std::move(resolvedDefinition)) {
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

std::shared_ptr<const NodeDefinition> DataNode::getDefinition() const {
    if (const RuntimeValue* unresolved =
            std::get_if<RuntimeValue>(&definition_)) {
        if (unresolved->isNil()) {
            return nullptr;
        }
        definition_ = std::make_shared<const NodeDefinition>(*unresolved);
    }
    return std::get<std::shared_ptr<const NodeDefinition>>(definition_);
}
