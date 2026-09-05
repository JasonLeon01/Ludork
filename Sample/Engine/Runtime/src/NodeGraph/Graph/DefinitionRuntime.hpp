#pragma once

#include <Runtime/NodeGraph/Node.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <memory>
#include <string>
#include <unordered_map>

namespace ludork::runtime::graph_detail {

std::unordered_map<std::string, int> parseStartNodes(
    const RuntimeValue& startNodeValues);
const DataNode& requireCompiledDataNode(
    const std::shared_ptr<DataNode>& dataNode);

}  // namespace ludork::runtime::graph_detail
