#include "DefinitionRuntime.hpp"

#include "Internal.hpp"

#include <stdexcept>

namespace ludork::engine::graph_detail {

std::unordered_map<std::string, int> parseStartNodes(
    const RuntimeValue& startNodeValues) {
    const RuntimeValue::Map* startNodeMap =
        startNodeValues.isNil() ? nullptr
                                : startNodeValues.getIf<RuntimeValue::Map>();
    if (!startNodeValues.isNil() && startNodeMap == nullptr) {
        throw std::invalid_argument("Graph startNodes must be a map");
    }
    std::unordered_map<std::string, int> result;
    if (startNodeMap == nullptr) {
        return result;
    }
    for (const auto& [key, value] : *startNodeMap) {
        if (value.isNil()) {
            continue;
        }
        const std::optional<int> index = integerValue(value);
        if (!index.has_value()) {
            throw std::runtime_error("Start node for key '" + key +
                                     "' must be an integer");
        }
        result.emplace(key, *index);
    }
    return result;
}

const DataNode& requireCompiledDataNode(
    const std::shared_ptr<DataNode>& dataNode) {
    if (dataNode == nullptr) {
        throw std::runtime_error("Graph contains a nil DataNode");
    }
    if (dataNode->getResolvedDefinition().isNil()) {
        throw std::runtime_error("DataNode '" + dataNode->nodeFunction +
                                 "' is not compiled");
    }
    return *dataNode;
}

}  // namespace ludork::engine::graph_detail
