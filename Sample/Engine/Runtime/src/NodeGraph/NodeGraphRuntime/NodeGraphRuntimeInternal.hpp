#pragma once

#include <Runtime/RuntimeSession.hpp>
#include <Runtime/NodeGraph/Types.hpp>
#include <span>

class Node;
class Graph;

namespace ludork::runtime::node_graph_detail {

struct NodeGraphContextObjects {
    RuntimeHandle localGraph;
    RuntimeHandle graph;
};

NodeGraphContextObjects createNodeGraphContext(RuntimeScope& scope,
                                               const RuntimeValue& parentClass,
                                               const RuntimeValue& parent);
enum class NodeGraphValueRead {
    Retained,
    Snapshot
};
RuntimeValue getNodeGraphContextValue(
    RuntimeScope& scope, const RuntimeHandle& context, const std::string& key,
    NodeGraphValueRead mode = NodeGraphValueRead::Retained);
void setNodeGraphContextValue(RuntimeScope& scope, const RuntimeHandle& context,
                              const std::string& key,
                              const RuntimeValue& value);
RuntimeValue getNodeGraphContextParent(RuntimeScope& scope,
                                       const RuntimeValue& graph);
void setNodeGraphContextParent(RuntimeScope& scope, const RuntimeValue& graph,
                               const RuntimeValue& parent);
std::shared_ptr<Node> createNodeGraphNode(
    RuntimeScope& scope, const RuntimeValue& nodeModel,
    const std::shared_ptr<Graph>& graph, const RuntimeValue& parent,
    const std::string& nodeFunction, const RuntimeHandle& fallback,
    const RuntimeValue::Array& parameters);
NodeResult invokeNodeGraphCallable(RuntimeScope& scope,
                                   const RuntimeHandle& callable,
                                   const RuntimeValue& self,
                                   std::span<const RuntimeValue> arguments,
                                   const RuntimeHandle& context);
RuntimeHandle nodeGraphRefLocal(RuntimeScope& scope,
                                const RuntimeHandle& callable);
NodeGraphConditionResult evaluateNodeGraphCondition(
    RuntimeScope& scope, const RuntimeHandle& condition);
NodeCache readNodeGraphCache(RuntimeScope& scope, const RuntimeHandle& cache);
void writeNodeGraphCache(RuntimeScope& scope, const RuntimeHandle& cache,
                         const NodeCache& values);
void clearNodeGraphRuntimeCaches();

}  // namespace ludork::runtime::node_graph_detail
