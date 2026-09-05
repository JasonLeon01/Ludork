#include <Runtime/NodeGraph/NodeGraphRuntime.hpp>
#include <Runtime/RuntimeReference.hpp>
#include <Runtime/RuntimeSession.hpp>

#include <Runtime/NodeGraph/Graph.hpp>
#include <Runtime/NodeGraph/Node.hpp>
#include <NodeGraph/NodeGraphRuntime/NodeGraphRuntimeInternal.hpp>

#include <utility>

using namespace ludork::runtime::reference;

NodeGraphRuntimeContext NodeGraphRuntimeFacade::createContext(
    const RuntimeValue& parentClass, const RuntimeValue& parent) const {
    ludork::runtime::RuntimeScope runtime;
    const ludork::runtime::node_graph_detail::NodeGraphContextObjects context =
        ludork::runtime::node_graph_detail::createNodeGraphContext(
            retain(parentClass), retain(parent));
    return {identity(context.localGraph), snapshot(context.graph)};
}

RuntimeValue NodeGraphRuntimeFacade::getContextValue(
    const RuntimeIdentityPtr& context, const std::string& key) const {
    ludork::runtime::RuntimeScope runtime;
    return snapshot(
        ludork::runtime::node_graph_detail::getNodeGraphContextValue(
            retain(makeValue(context)), key));
}

void NodeGraphRuntimeFacade::setContextValue(const RuntimeIdentityPtr& context,
                                             const std::string& key,
                                             const RuntimeValue& value) const {
    ludork::runtime::RuntimeScope runtime;
    ludork::runtime::node_graph_detail::setNodeGraphContextValue(
        retain(makeValue(context)), key, retain(value));
}

RuntimeValue NodeGraphRuntimeFacade::getContextParent(
    const RuntimeValue& graph) const {
    ludork::runtime::RuntimeScope runtime;
    return snapshot(
        ludork::runtime::node_graph_detail::getNodeGraphContextParent(
            retain(graph)));
}

void NodeGraphRuntimeFacade::setContextParent(
    const RuntimeValue& graph, const RuntimeValue& parent) const {
    ludork::runtime::RuntimeScope runtime;
    ludork::runtime::node_graph_detail::setNodeGraphContextParent(
        retain(graph), retain(parent));
}

std::shared_ptr<Node> NodeGraphRuntimeFacade::createNode(
    const RuntimeValue& nodeModel, const std::shared_ptr<Graph>& graph,
    const RuntimeValue& parent, const std::string& nodeFunction,
    const RuntimeIdentityPtr& fallback,
    const RuntimeValue::Array& parameters) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::node_graph_detail::createNodeGraphNode(
        retain(nodeModel), retain(makeValue(graph)), retain(parent),
        nodeFunction, retain(makeValue(fallback)),
        retain(makeValue(parameters)));
}

NodeResult NodeGraphRuntimeFacade::invoke(
    const RuntimeIdentityPtr& callable, const RuntimeValue& self,
    const RuntimeValue::Array& arguments,
    const RuntimeIdentityPtr& context) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::node_graph_detail::invokeNodeGraphCallable(
        retain(makeValue(callable)), retain(self), retain(makeValue(arguments)),
        retain(makeValue(context)));
}

RuntimeIdentityPtr NodeGraphRuntimeFacade::refLocal(
    const RuntimeIdentityPtr& callable) const {
    ludork::runtime::RuntimeScope runtime;
    return identity(ludork::runtime::node_graph_detail::nodeGraphRefLocal(
        retain(makeValue(callable))));
}

NodeGraphConditionResult NodeGraphRuntimeFacade::evaluateCondition(
    const RuntimeIdentityPtr& condition) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::node_graph_detail::evaluateNodeGraphCondition(
        retain(makeValue(condition)));
}

NodeCache NodeGraphRuntimeFacade::readCache(
    const RuntimeIdentityPtr& cache) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::node_graph_detail::readNodeGraphCache(
        retain(makeValue(cache)));
}

void NodeGraphRuntimeFacade::writeCache(const RuntimeIdentityPtr& cache,
                                        const NodeCache& values) const {
    ludork::runtime::RuntimeScope runtime;
    ludork::runtime::node_graph_detail::writeNodeGraphCache(
        retain(makeValue(cache)), values);
}

NodeGraphRuntimeFacade& nodeGraphRuntime() {
    static NodeGraphRuntimeFacade facade;
    return facade;
}
