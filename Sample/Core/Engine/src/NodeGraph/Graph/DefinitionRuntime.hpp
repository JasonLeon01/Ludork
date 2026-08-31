#pragma once

#include <NodeGraph/Node.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <memory>
#include <string>
#include <unordered_map>

namespace ludork::engine::graph_detail {

std::unordered_map<std::string, int> parseStartNodes(
    const RuntimeValue& startNodeValues);
const DataNode& requireCompiledDataNode(
    const std::shared_ptr<DataNode>& dataNode);

}  // namespace ludork::engine::graph_detail
