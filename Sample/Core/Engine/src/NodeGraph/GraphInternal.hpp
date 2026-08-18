#pragma once

#include <NodeGraph/Graph.hpp>

namespace ludork::engine::graph_detail {

RuntimeIdentityPtr identityValue(const RuntimeValue* value);
std::optional<int> integerValue(const RuntimeValue& value);
std::optional<NodeIndex> nodeIndexValue(const RuntimeValue& value);
std::vector<int> sortedPins(const std::unordered_map<int, Graph::NodeSource>& pins);
NodeCache decodeNodeCache(const RuntimeIdentityPtr& cacheIdentity);
void syncNodeCache(const RuntimeIdentityPtr& cacheIdentity,
                   const NodeCache& cache);

}  // namespace ludork::engine::graph_detail
