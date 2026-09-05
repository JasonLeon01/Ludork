#include "NodeGraphRuntimeInternal.hpp"
#include "RuntimeBindingTraits.hpp"
#include "RuntimeServiceInternals.hpp"
#include <Runtime/NodeGraph/Graph.hpp>
#include <Runtime/NodeGraph/Node.hpp>
#include <Runtime/Detail/RuntimeServices.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>

#include <climits>
#include <exception>
#include <limits>
#include <stdexcept>

namespace ludork::runtime::node_graph_detail {
namespace {
constexpr const char* NODEGRAPH_REF_LOCALS_KEY =
    "Ludork.Runtime.NodeGraph.refLocals";
constexpr const char* NODEGRAPH_CONTEXTS_KEY =
    "Ludork.Runtime.NodeGraph.contexts";

struct StackRestore {
    lua_State* state;
    int top;
    ~StackRestore() {
        lua_settop(state, top);
    }
};

sol::object write(sol::state_view lua, const RuntimeValue& value) {
    return binding::writeLuaValue(lua, value);
}
RuntimeValue snapshot(const sol::object& value) {
    return binding::readLuaValue<RuntimeValue>(value);
}
RuntimeHandle capture(const sol::object& value) {
    return RuntimeHandle(
        binding::readOpaqueIdentity<RuntimeIdentityPtr>(value));
}
sol::table requireTable(const sol::object& value) {
    if (!value.is<sol::table>()) {
        throw std::invalid_argument("Runtime value must be a table");
    }
    return value.as<sol::table>();
}
bool isCallable(sol::state_view lua, const sol::object& value) {
    if (value.get_type() == sol::type::function) {
        return true;
    }
    if (value.get_type() != sol::type::table &&
        value.get_type() != sol::type::userdata) {
        return false;
    }
    return detail::objectMetatable(lua, value)
               .raw_get<sol::object>("__call")
               .get_type() == sol::type::function;
}
std::size_t packedCount(const sol::table& values) {
    std::int64_t count = 0;
    return binding::luaIntegerValue(values.raw_get<sol::object>("n"), count) &&
                   count >= 0
               ? static_cast<std::size_t>(count)
               : values.size();
}
NodeResult collect(lua_State* state, int base, int count) {
    NodeResult result;
    result.count = static_cast<std::size_t>(count);
    result.values.reserve(result.count);
    for (int index = 1; index <= count; ++index) {
        result.values.push_back(
            snapshot(sol::stack::get<sol::object>(state, base + index)));
    }
    return result;
}
NodeResult readPacked(const sol::object& value) {
    NodeResult result;
    if (!value.is<sol::table>()) {
        return {{snapshot(value)}, 1};
    }
    const sol::table table = value.as<sol::table>();
    result.count = packedCount(table);
    if (result.count > static_cast<std::size_t>(INT_MAX)) {
        throw std::length_error("Node graph cache value count overflow");
    }
    result.values.reserve(result.count);
    for (std::size_t index = 1; index <= result.count; ++index) {
        result.values.push_back(snapshot(table.raw_get<sol::object>(index)));
    }
    return result;
}
sol::table contextTarget(const sol::object& value) {
    const sol::table table = requireTable(value);
    const sol::object graph = table.raw_get<sol::object>("__graph__");
    return graph.is<sol::table>() ? graph.as<sol::table>() : table;
}
NodeIndex cacheKey(const sol::object& value) {
    if (value.get_type() == sol::type::string) {
        return value.as<std::string>();
    }
    std::int64_t index = 0;
    if (!binding::luaIntegerValue(value, index)) {
        throw std::invalid_argument(
            "Node graph cache keys must be strings or integers");
    }
    if (index < INT_MIN || index > INT_MAX) {
        throw std::out_of_range("Node graph cache integer key is out of range");
    }
    return static_cast<int>(index);
}
}  // namespace

NodeGraphContextObjects createNodeGraphContext(
    RuntimeScope& scope, const RuntimeValue& parentClass,
    const RuntimeValue& parentValue) {
    sol::state_view lua(scope.state());
    sol::table context = lua.create_table();
    context[sol::metatable_key] = lua.create_table();
    sol::table graph = lua.create_table();
    graph.raw_set("parentClass", write(lua, parentClass));
    graph.raw_set("localGraph", context);
    sol::table parent = detail::createWeakTable(lua, "v");
    parent.raw_set(1, write(lua, parentValue));
    sol::table meta = lua.create_table();
    meta.set_function(
        "__index",
        [parent](const sol::object&, const sol::object& key) -> sol::object {
            return key.get_type() == sol::type::string &&
                           key.as<std::string>() == "parent"
                       ? parent.raw_get<sol::object>(1)
                       : detail::nilObject(sol::state_view(key.lua_state()));
        });
    meta.set_function("__newindex",
                      [parent](sol::table target, const sol::object& key,
                               const sol::object& value) mutable {
                          if (key.get_type() == sol::type::string &&
                              key.as<std::string>() == "parent") {
                              parent.raw_set(1, value);
                          } else {
                              target.raw_set(key, value);
                          }
                      });
    graph[sol::metatable_key] = meta;
    context.raw_set("__graph__", graph);
    detail::registryTable(lua, NODEGRAPH_CONTEXTS_KEY, "k")
        .raw_set(graph, context);
    return {capture(sol::make_object(lua, context)),
            capture(sol::make_object(lua, graph))};
}
RuntimeValue getNodeGraphContextValue(RuntimeScope& scope,
                                      const RuntimeHandle& context,
                                      const std::string& key,
                                      NodeGraphValueRead mode) {
    const sol::object value =
        requireTable(write(sol::state_view(scope.state()), context))
            .raw_get<sol::object>(key);
    return mode == NodeGraphValueRead::Snapshot
               ? snapshot(value)
               : detail::readRuntimeReference(value);
}
void setNodeGraphContextValue(RuntimeScope& scope, const RuntimeHandle& context,
                              const std::string& key,
                              const RuntimeValue& value) {
    sol::state_view lua(scope.state());
    requireTable(write(lua, context)).raw_set(key, write(lua, value));
}
RuntimeValue getNodeGraphContextParent(RuntimeScope& scope,
                                       const RuntimeValue& graph) {
    sol::state_view lua(scope.state());
    return snapshot(detail::runtimeIndex(
        lua, sol::make_object(lua, contextTarget(write(lua, graph))),
        sol::make_object(lua, "parent"), false));
}
void setNodeGraphContextParent(RuntimeScope& scope, const RuntimeValue& graph,
                               const RuntimeValue& parent) {
    sol::state_view lua(scope.state());
    detail::runtimeAssign(
        lua, sol::make_object(lua, contextTarget(write(lua, graph))),
        sol::make_object(lua, "parent"), write(lua, parent), false);
}
std::shared_ptr<Node> createNodeGraphNode(
    RuntimeScope& scope, const RuntimeValue& nodeModel,
    const std::shared_ptr<Graph>& graph, const RuntimeValue& parent,
    const std::string& nodeFunction, const RuntimeHandle& fallback,
    const RuntimeValue::Array& parameters) {
    if (nodeModel.isNil()) {
        return nullptr;
    }
    sol::state_view lua(scope.state());
    const sol::object constructor = detail::runtimeIndex(
        lua, write(lua, nodeModel), sol::make_object(lua, "new"), false);
    if (constructor.get_type() != sol::type::function) {
        return nullptr;
    }
    const int base = lua_gettop(scope.state());
    StackRestore restore{scope.state(), base};
    const int count = detail::invokeRuntimeFunction(
        scope.state(), constructor,
        {write(lua, RuntimeValue(graph)), write(lua, parent),
         sol::make_object(lua, nodeFunction), write(lua, fallback),
         binding::writeLuaValue(lua, parameters)},
        "node constructor arguments");
    if (count == 0 || lua_isnil(scope.state(), base + 1)) {
        return nullptr;
    }
    std::shared_ptr<RuntimeObject> object;
    binding::tryReadSharedPointer(
        sol::stack::get<sol::object>(scope.state(), base + 1), object);
    const auto node = std::dynamic_pointer_cast<Node>(object);
    if (node == nullptr) {
        throw std::runtime_error(
            "Node model constructor must return an Engine.Node or nil");
    }
    return node;
}
NodeResult invokeNodeGraphCallable(RuntimeScope& scope,
                                   const RuntimeHandle& callable,
                                   const RuntimeValue& self,
                                   std::span<const RuntimeValue> arguments,
                                   const RuntimeHandle& context) {
    sol::state_view lua(scope.state());
    const sol::object function = write(lua, callable);
    const sol::object receiver = write(lua, self);
    const sol::object local = write(lua, context);
    if (!isCallable(lua, function)) {
        throw std::runtime_error("Node graph value is not callable");
    }
    const std::size_t selfCount = binding::isNil(receiver) ? 0 : 1;
    if (arguments.size() > static_cast<std::size_t>(INT_MAX) - selfCount) {
        throw std::length_error("Node graph argument count overflow");
    }
    std::vector<sol::object> values;
    values.reserve(arguments.size() + selfCount);
    if (selfCount) {
        values.push_back(receiver);
    }
    for (const RuntimeValue& value : arguments) {
        values.push_back(write(lua, value));
    }
    sol::table refLocals =
        detail::registryTable(lua, NODEGRAPH_REF_LOCALS_KEY, "k");
    const sol::object oldRef = refLocals.raw_get<sol::object>(function);
    sol::object oldActive = detail::nilObject(lua);
    const bool hasContext = local.is<sol::table>();
    if (hasContext) {
        sol::table table = local.as<sol::table>();
        oldActive = table.raw_get<sol::object>("__activeNodeFunction__");
        table.raw_set("__activeNodeFunction__", function);
        refLocals.raw_set(function, local);
    }
    const int base = lua_gettop(scope.state());
    StackRestore restore{scope.state(), base};
    int count = 0;
    std::exception_ptr failure;
    try {
        count = detail::invokeRuntimeFunction(scope.state(), function, values,
                                              "node graph arguments");
    } catch (...) {
        failure = std::current_exception();
    }
    if (hasContext) {
        local.as<sol::table>().raw_set("__activeNodeFunction__", oldActive);
        refLocals.raw_set(function, oldRef);
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
    return collect(scope.state(), base, count);
}
RuntimeHandle nodeGraphRefLocal(RuntimeScope& scope,
                                const RuntimeHandle& callable) {
    sol::state_view lua(scope.state());
    return capture(detail::registryTable(lua, NODEGRAPH_REF_LOCALS_KEY, "k")
                       .raw_get<sol::object>(write(lua, callable)));
}
NodeGraphConditionResult evaluateNodeGraphCondition(
    RuntimeScope& scope, const RuntimeHandle& condition) {
    sol::state_view lua(scope.state());
    const sol::object callable = write(lua, condition);
    if (!isCallable(lua, callable)) {
        throw std::invalid_argument("Runtime value is not callable");
    }
    const int base = lua_gettop(scope.state());
    StackRestore restore{scope.state(), base};
    const int count = detail::invokeRuntimeFunction(scope.state(), callable, {},
                                                    "node graph condition");
    NodeGraphConditionResult result;
    if (count != 0 && !lua_isnil(scope.state(), base + 1)) {
        const sol::object first =
            sol::stack::get<sol::object>(scope.state(), base + 1);
        if (first.is<sol::table>()) {
            const sol::table table = first.as<sol::table>();
            std::int64_t length = 0;
            if ((binding::luaIntegerValue(table.raw_get<sol::object>("n"),
                                          length) &&
                 length >= 0) ||
                table.size() > 0) {
                result.result = readPacked(first);
            } else {
                for (const auto& entry : table) {
                    result.result.values.push_back(snapshot(entry.second));
                }
                result.result.count = result.result.values.size();
            }
        } else {
            result.result = {{snapshot(first)}, 1};
        }
    }
    lua_settop(scope.state(), base);
    if (callable.is<sol::table>()) {
        const sol::object finished = detail::runtimeIndex(
            lua, callable, sol::make_object(lua, "isFinished"), false);
        if (isCallable(lua, finished)) {
            const int finishedCount = detail::invokeRuntimeFunction(
                scope.state(), finished, {callable},
                "node graph condition isFinished");
            if (finishedCount != 1 ||
                lua_type(scope.state(), base + 1) != LUA_TBOOLEAN) {
                throw std::runtime_error(
                    "Node graph condition isFinished must return exactly one "
                    "boolean");
            }
            result.finished = lua_toboolean(scope.state(), base + 1) != 0;
        }
    }
    return result;
}
NodeCache readNodeGraphCache(RuntimeScope& scope, const RuntimeHandle& cache) {
    sol::state_view lua(scope.state());
    const sol::object table = write(lua, cache);
    NodeCache result;
    if (!table.is<sol::table>()) {
        return result;
    }
    for (const auto& entry : table.as<sol::table>()) {
        result.emplace(cacheKey(entry.first), readPacked(entry.second));
    }
    return result;
}
void writeNodeGraphCache(RuntimeScope& scope, const RuntimeHandle& cache,
                         const NodeCache& values) {
    sol::state_view lua(scope.state());
    const sol::object value = write(lua, cache);
    if (!value.is<sol::table>()) {
        return;
    }
    sol::table table = value.as<sol::table>();
    std::vector<sol::object> keys;
    for (const auto& entry : table) {
        keys.push_back(entry.first);
    }
    for (const sol::object& key : keys) {
        table.raw_set(key, sol::lua_nil);
    }
    for (const auto& [key, result] : values) {
        if (result.count > static_cast<std::size_t>(INT_MAX)) {
            throw std::length_error("Node graph cache value count overflow");
        }
        sol::table packed = lua.create_table(static_cast<int>(result.count), 1);
        packed.raw_set("n", result.count);
        for (std::size_t index = 0;
             index < result.count && index < result.values.size(); ++index) {
            packed.raw_set(index + 1, write(lua, result.values[index]));
        }
        std::visit(
            [&](const auto& index) {
                table.raw_set(index, packed);
            },
            key);
    }
}
void clearNodeGraphRuntimeCaches() {
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    lua.registry().raw_set(NODEGRAPH_REF_LOCALS_KEY, sol::lua_nil);
    lua.registry().raw_set(NODEGRAPH_CONTEXTS_KEY, sol::lua_nil);
}
}  // namespace ludork::runtime::node_graph_detail
