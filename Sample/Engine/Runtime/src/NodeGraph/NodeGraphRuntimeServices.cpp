#include <Runtime/NodeGraph/NodeGraphRuntime.hpp>
#include "NodeGraphRuntime/NodeGraphRuntimeInternal.hpp"

using namespace ludork::runtime;
using namespace ludork::runtime::node_graph_detail;

NodeGraphRuntimeContext NodeGraphRuntimeFacade::createContext(
    const RuntimeValue& parentClass, const RuntimeValue& parent) const {
    RuntimeScope scope;
    const auto context = createNodeGraphContext(scope, parentClass, parent);
    return {context.localGraph.identity(), RuntimeValue(context.graph)};
}
RuntimeValue NodeGraphRuntimeFacade::getContextValue(
    const RuntimeIdentityPtr& context, const std::string& key) const {
    RuntimeScope scope;
    return getNodeGraphContextValue(scope, RuntimeHandle(context), key,
                                    NodeGraphValueRead::Snapshot);
}
void NodeGraphRuntimeFacade::setContextValue(const RuntimeIdentityPtr& context,
                                             const std::string& key,
                                             const RuntimeValue& value) const {
    RuntimeScope scope;
    setNodeGraphContextValue(scope, RuntimeHandle(context), key, value);
}
RuntimeValue NodeGraphRuntimeFacade::getContextParent(
    const RuntimeValue& graph) const {
    RuntimeScope scope;
    return getNodeGraphContextParent(scope, graph);
}
void NodeGraphRuntimeFacade::setContextParent(
    const RuntimeValue& graph, const RuntimeValue& parent) const {
    RuntimeScope scope;
    setNodeGraphContextParent(scope, graph, parent);
}
std::shared_ptr<Node> NodeGraphRuntimeFacade::createNode(
    const RuntimeValue& nodeModel, const std::shared_ptr<Graph>& graph,
    const RuntimeValue& parent, const std::string& nodeFunction,
    const RuntimeIdentityPtr& fallback,
    const RuntimeValue::Array& parameters) const {
    RuntimeScope scope;
    return createNodeGraphNode(scope, nodeModel, graph, parent, nodeFunction,
                               RuntimeHandle(fallback), parameters);
}
NodeResult NodeGraphRuntimeFacade::invoke(
    const RuntimeIdentityPtr& callable, const RuntimeValue& self,
    const RuntimeValue::Array& arguments,
    const RuntimeIdentityPtr& context) const {
    RuntimeScope scope;
    return invokeNodeGraphCallable(scope, RuntimeHandle(callable), self,
                                   arguments, RuntimeHandle(context));
}
RuntimeIdentityPtr NodeGraphRuntimeFacade::refLocal(
    const RuntimeIdentityPtr& callable) const {
    RuntimeScope scope;
    return nodeGraphRefLocal(scope, RuntimeHandle(callable)).identity();
}
NodeGraphConditionResult NodeGraphRuntimeFacade::evaluateCondition(
    const RuntimeIdentityPtr& condition) const {
    RuntimeScope scope;
    return evaluateNodeGraphCondition(scope, RuntimeHandle(condition));
}
NodeCache NodeGraphRuntimeFacade::readCache(
    const RuntimeIdentityPtr& cache) const {
    RuntimeScope scope;
    return readNodeGraphCache(scope, RuntimeHandle(cache));
}
void NodeGraphRuntimeFacade::writeCache(const RuntimeIdentityPtr& cache,
                                        const NodeCache& values) const {
    RuntimeScope scope;
    writeNodeGraphCache(scope, RuntimeHandle(cache), values);
}
NodeGraphRuntimeFacade& nodeGraphRuntime() {
    static NodeGraphRuntimeFacade facade;
    return facade;
}
