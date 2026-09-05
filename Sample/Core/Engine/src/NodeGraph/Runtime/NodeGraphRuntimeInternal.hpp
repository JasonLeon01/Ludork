#pragma once

#include <Runtime/RuntimeReference.hpp>

#include <NodeGraph/NodeGraphRuntime.hpp>

namespace ludork::engine::runtime_detail {

using namespace ludork::runtime::reference;

struct NodeGraphContextObjects {
    RuntimeValue localGraph;
    RuntimeValue graph;
};

NodeGraphContextObjects createNodeGraphContext(const RuntimeValue& parentClass,
                                               const RuntimeValue& parent);
RuntimeValue getNodeGraphContextValue(const RuntimeValue& context,
                                      const std::string& key);
void setNodeGraphContextValue(const RuntimeValue& context,
                              const std::string& key,
                              const RuntimeValue& value);
RuntimeValue getNodeGraphContextParent(const RuntimeValue& graph);
void setNodeGraphContextParent(const RuntimeValue& graph,
                               const RuntimeValue& parent);
std::shared_ptr<Node> createNodeGraphNode(const RuntimeValue& nodeModel,
                                          const RuntimeValue& graph,
                                          const RuntimeValue& parent,
                                          const std::string& nodeFunction,
                                          const RuntimeValue& fallback,
                                          const RuntimeValue& parameters);
NodeResult invokeNodeGraphCallable(const RuntimeValue& callable,
                                   const RuntimeValue& self,
                                   const RuntimeValue& arguments,
                                   const RuntimeValue& context);
RuntimeValue nodeGraphRefLocal(const RuntimeValue& callable);
NodeGraphConditionResult evaluateNodeGraphCondition(
    const RuntimeValue& condition);
NodeCache readNodeGraphCache(const RuntimeValue& cache);
void writeNodeGraphCache(const RuntimeValue& cache, const NodeCache& values);
void clearNodeGraphRuntimeCaches();

}  // namespace ludork::engine::runtime_detail
