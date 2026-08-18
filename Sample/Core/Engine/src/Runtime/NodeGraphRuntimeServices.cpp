#include "RuntimeSubsystemServices.hpp"

namespace ludork::engine::runtime_detail {
namespace {

constexpr const char* NODEGRAPH_REF_LOCALS_KEY =
    "Ludork.Engine.NodeGraph.refLocals";

}  // namespace

const std::vector<std::string>& nodeGraphRuntimeServiceNames() {
    static const std::vector<std::string> names{
        "nodegraph.context",  "nodegraph.createNode", "nodegraph.invoke",
        "nodegraph.refLocal", "nodegraph.condition",  "nodegraph.cache",
    };
    return names;
}

ServiceDispatchResult dispatchNodeGraphRuntimeService(
    sol::this_state state, const std::string& operation,
    const RuntimeArguments& arguments) {
    sol::state_view lua(state);
    if (operation == "nodegraph.context") {
        return nodeGraphContext(lua, arguments);
    }
    if (operation == "nodegraph.createNode") {
        return createNodeGraphNode(lua, arguments);
    }
    if (operation == "nodegraph.invoke") {
        return invokeNodeGraphCallable(lua, arguments);
    }
    if (operation == "nodegraph.refLocal") {
        const sol::object callable = runtimeResolverArgument(lua, arguments, 1);
        const sol::object context =
            registryTable(lua, NODEGRAPH_REF_LOCALS_KEY, "k")
                .raw_get<sol::object>(callable);
        return runtimeResolverResult(lua, {context});
    }
    if (operation == "nodegraph.condition") {
        return evaluateNodeGraphCondition(lua, arguments);
    }
    if (operation == "nodegraph.cache") {
        return bridgeNodeGraphCache(lua, arguments);
    }
    return std::nullopt;
}

}  // namespace ludork::engine::runtime_detail
