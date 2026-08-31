#pragma once

#include <NodeGraph/Node.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ludork::engine::graph_detail {

std::optional<int> namedExecPinIndex(const NodeMemberMetadata& metadata,
                                     const std::string& pinName);
NodeResult emptyLoopResult(const NodeMemberMetadata& metadata);
std::vector<NodeResult> loopResults(const NodeMemberMetadata& metadata,
                                    const NodeResult& controlResult);

}  // namespace ludork::engine::graph_detail
