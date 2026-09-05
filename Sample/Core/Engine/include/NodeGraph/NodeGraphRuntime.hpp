#pragma once

#include <EngineRuntimeApi.hpp>
#include <NodeGraph/Types.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <memory>
#include <string>

class Graph;
class Node;

struct NodeGraphRuntimeContext {
    RuntimeIdentityPtr localGraph;
    RuntimeValue graph;
};

struct NodeGraphConditionResult {
    NodeResult result;
    bool finished = true;
};

class LUDORK_ENGINE_API NodeGraphRuntimeFacade {
public:
    NodeGraphRuntimeContext createContext(const RuntimeValue& parentClass,
                                          const RuntimeValue& parent) const;
    RuntimeValue getContextValue(const RuntimeIdentityPtr& context,
                                 const std::string& key) const;
    void setContextValue(const RuntimeIdentityPtr& context,
                         const std::string& key,
                         const RuntimeValue& value) const;
    RuntimeValue getContextParent(const RuntimeValue& graph) const;
    void setContextParent(const RuntimeValue& graph,
                          const RuntimeValue& parent) const;
    std::shared_ptr<Node> createNode(
        const RuntimeValue& nodeModel, const std::shared_ptr<Graph>& graph,
        const RuntimeValue& parent, const std::string& nodeFunction,
        const RuntimeIdentityPtr& fallback,
        const RuntimeValue::Array& parameters) const;
    NodeResult invoke(const RuntimeIdentityPtr& callable,
                      const RuntimeValue& self,
                      const RuntimeValue::Array& arguments,
                      const RuntimeIdentityPtr& context) const;
    RuntimeIdentityPtr refLocal(const RuntimeIdentityPtr& callable) const;
    NodeGraphConditionResult evaluateCondition(
        const RuntimeIdentityPtr& condition) const;
    NodeCache readCache(const RuntimeIdentityPtr& cache) const;
    void writeCache(const RuntimeIdentityPtr& cache,
                    const NodeCache& values) const;
};

LUDORK_ENGINE_API NodeGraphRuntimeFacade& nodeGraphRuntime();
