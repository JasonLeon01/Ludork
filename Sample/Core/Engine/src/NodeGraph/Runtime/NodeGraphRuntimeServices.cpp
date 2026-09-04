#include <Runtime/RuntimeSession.hpp>

#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include <NodeGraph/Graph.hpp>
#include <NodeGraph/Node.hpp>
#include <NodeGraph/NodeGraphRuntime.hpp>

#include <utility>

namespace {

sol::object writeRuntimeValue(sol::state_view lua, const RuntimeValue& value) {
    return ludork::runtime::binding::writeLuaValue(lua, value);
}

sol::object writeRuntimeIdentity(sol::state_view lua,
                                 const RuntimeIdentityPtr& value) {
    return ludork::runtime::binding::writeLuaValue(lua, value);
}

}  // namespace

NodeGraphRuntimeContext NodeGraphRuntimeFacade::createContext(
    const RuntimeValue& parentClass, const RuntimeValue& parent) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    const ludork::engine::runtime_detail::NodeGraphContextObjects context =
        ludork::engine::runtime_detail::createNodeGraphContext(
            lua, writeRuntimeValue(lua, parentClass),
            writeRuntimeValue(lua, parent));
    return {
        ludork::runtime::binding::readLuaValue<RuntimeIdentityPtr>(
            context.localGraph),
        ludork::runtime::binding::readLuaValue<RuntimeValue>(context.graph)};
}

RuntimeValue NodeGraphRuntimeFacade::getContextValue(
    const RuntimeIdentityPtr& context, const std::string& key) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    return ludork::runtime::binding::readLuaValue<RuntimeValue>(
        ludork::engine::runtime_detail::getNodeGraphContextValue(
            lua, writeRuntimeIdentity(lua, context), key));
}

void NodeGraphRuntimeFacade::setContextValue(const RuntimeIdentityPtr& context,
                                             const std::string& key,
                                             const RuntimeValue& value) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    ludork::engine::runtime_detail::setNodeGraphContextValue(
        lua, writeRuntimeIdentity(lua, context), key,
        writeRuntimeValue(lua, value));
}

RuntimeValue NodeGraphRuntimeFacade::getContextParent(
    const RuntimeValue& graph) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    return ludork::runtime::binding::readLuaValue<RuntimeValue>(
        ludork::engine::runtime_detail::getNodeGraphContextParent(
            lua, writeRuntimeValue(lua, graph)));
}

void NodeGraphRuntimeFacade::setContextParent(
    const RuntimeValue& graph, const RuntimeValue& parent) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    ludork::engine::runtime_detail::setNodeGraphContextParent(
        lua, writeRuntimeValue(lua, graph), writeRuntimeValue(lua, parent));
}

std::shared_ptr<Node> NodeGraphRuntimeFacade::createNode(
    const RuntimeValue& nodeModel, const std::shared_ptr<Graph>& graph,
    const RuntimeValue& parent, const std::string& nodeFunction,
    const RuntimeIdentityPtr& fallback,
    const RuntimeValue::Array& parameters) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    return ludork::engine::runtime_detail::createNodeGraphNode(
        lua, writeRuntimeValue(lua, nodeModel),
        ludork::runtime::binding::writeLuaValue(lua, graph),
        writeRuntimeValue(lua, parent), nodeFunction,
        writeRuntimeIdentity(lua, fallback),
        ludork::runtime::binding::writeLuaValue(lua, parameters));
}

NodeResult NodeGraphRuntimeFacade::invoke(
    const RuntimeIdentityPtr& callable, const RuntimeValue& self,
    const RuntimeValue::Array& arguments,
    const RuntimeIdentityPtr& context) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    return ludork::engine::runtime_detail::invokeNodeGraphCallable(
        lua, writeRuntimeIdentity(lua, callable), writeRuntimeValue(lua, self),
        ludork::runtime::binding::writeLuaValue(lua, arguments),
        writeRuntimeIdentity(lua, context));
}

RuntimeIdentityPtr NodeGraphRuntimeFacade::refLocal(
    const RuntimeIdentityPtr& callable) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    return ludork::runtime::binding::readLuaValue<RuntimeIdentityPtr>(
        ludork::engine::runtime_detail::nodeGraphRefLocal(
            lua, writeRuntimeIdentity(lua, callable)));
}

NodeGraphConditionResult NodeGraphRuntimeFacade::evaluateCondition(
    const RuntimeIdentityPtr& condition) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    return ludork::engine::runtime_detail::evaluateNodeGraphCondition(
        lua, writeRuntimeIdentity(lua, condition));
}

NodeCache NodeGraphRuntimeFacade::readCache(
    const RuntimeIdentityPtr& cache) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    return ludork::engine::runtime_detail::readNodeGraphCache(
        lua, writeRuntimeIdentity(lua, cache));
}

void NodeGraphRuntimeFacade::writeCache(const RuntimeIdentityPtr& cache,
                                        const NodeCache& values) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    ludork::engine::runtime_detail::writeNodeGraphCache(
        lua, writeRuntimeIdentity(lua, cache), values);
}

NodeGraphRuntimeFacade& nodeGraphRuntime() {
    static NodeGraphRuntimeFacade facade;
    return facade;
}
