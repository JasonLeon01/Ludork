#include <Runtime/NodeGraph/DataNode.hpp>

#include <stdexcept>
#include <utility>

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
