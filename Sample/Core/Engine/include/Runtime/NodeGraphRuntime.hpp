#pragma once

#include <EngineRuntimeApi.hpp>
#include <NodeGraph/Types.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <sol2/sol.hpp>

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

namespace ludork::engine::runtime_detail {

struct NodeGraphContextObjects {
    sol::object localGraph;
    sol::object graph;
};

NodeGraphContextObjects createNodeGraphContext(sol::state_view lua,
                                               const sol::object& parentClass,
                                               const sol::object& parent);
sol::object getNodeGraphContextValue(sol::state_view lua,
                                     const sol::object& context,
                                     const std::string& key);
void setNodeGraphContextValue(sol::state_view lua, const sol::object& context,
                              const std::string& key, const sol::object& value);
sol::object getNodeGraphContextParent(sol::state_view lua,
                                      const sol::object& graph);
void setNodeGraphContextParent(sol::state_view lua, const sol::object& graph,
                               const sol::object& parent);
std::shared_ptr<Node> createNodeGraphNode(
    sol::state_view lua, const sol::object& nodeModel, const sol::object& graph,
    const sol::object& parent, const std::string& nodeFunction,
    const sol::object& fallback, const sol::object& parameters);
NodeResult invokeNodeGraphCallable(sol::state_view lua,
                                   const sol::object& callable,
                                   const sol::object& self,
                                   const sol::object& arguments,
                                   const sol::object& context);
sol::object nodeGraphRefLocal(sol::state_view lua, const sol::object& callable);
NodeGraphConditionResult evaluateNodeGraphCondition(
    sol::state_view lua, const sol::object& condition);
NodeCache readNodeGraphCache(sol::state_view lua, const sol::object& cache);
void writeNodeGraphCache(sol::state_view lua, const sol::object& cache,
                         const NodeCache& values);
void clearNodeGraphRuntimeCaches(sol::state_view lua);

}  // namespace ludork::engine::runtime_detail
