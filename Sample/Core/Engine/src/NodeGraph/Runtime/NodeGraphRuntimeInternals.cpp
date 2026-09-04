#include <Runtime/Detail/RuntimeServices.hpp>

#include <ClassServices.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include <NodeGraph/Graph.hpp>
#include <NodeGraph/Node.hpp>
#include <NodeGraph/NodeGraphRuntime.hpp>

extern "C" {
#include <lua.h>
}

#include <climits>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ludork::engine::runtime_detail {

using ludork::runtime::detail::ensureRuntimeLuaStack;
using ludork::runtime::detail::invokeRuntimeFunction;
using ludork::runtime::detail::nilObject;
using ludork::runtime::detail::objectMetatable;
using ludork::runtime::detail::registryTable;

namespace {

constexpr const char* NODEGRAPH_REF_LOCALS_KEY =
    "Ludork.Engine.NodeGraph.refLocals";
constexpr const char* NODEGRAPH_CONTEXTS_KEY =
    "Ludork.Engine.NodeGraph.contexts";

sol::object protectedIndex(sol::state_view lua, const sol::object& target,
                           const sol::object& key) {
    return ludork::standard::class_runtime::protectedGet(lua, target, key);
}

void protectedAssign(sol::state_view lua, const sol::object& target,
                     const sol::object& key, const sol::object& value) {
    ludork::standard::class_runtime::protectedSet(lua, target, key, value);
}

sol::table createWeakTable(sol::state_view lua, const char* mode) {
    sol::table result = lua.create_table();
    sol::table metatable = lua.create_table();
    metatable["__mode"] = mode;
    result[sol::metatable_key] = metatable;
    return result;
}

bool luaBoolean(const sol::object& value) {
    return value.is<bool>() && value.as<bool>();
}

bool isRuntimeCallable(sol::state_view lua, const sol::object& value) {
    if (value.is<sol::function>()) {
        return true;
    }
    if (value.get_type() != sol::type::table &&
        value.get_type() != sol::type::userdata) {
        return false;
    }
    const sol::table metatable = objectMetatable(lua, value);
    return metatable.raw_get<sol::object>("__call").is<sol::function>();
}

std::size_t nodeGraphPackedCount(const sol::table& values) {
    const sol::object rawCount = values.raw_get<sol::object>("n");
    return rawCount.is<std::size_t>() ? rawCount.as<std::size_t>()
                                      : values.size();
}

std::vector<sol::object> nodeGraphCallArguments(
    const sol::object& self, const sol::object& rawArguments) {
    const std::size_t selfCount =
        self.valid() && self.get_type() != sol::type::lua_nil ? 1 : 0;
    const std::size_t count =
        rawArguments.is<sol::table>()
            ? nodeGraphPackedCount(rawArguments.as<sol::table>())
            : 0;
    if (count > static_cast<std::size_t>(INT_MAX) - selfCount) {
        throw std::length_error("Node graph argument count overflow");
    }
    std::vector<sol::object> arguments;
    arguments.reserve(selfCount + count);
    if (selfCount != 0) {
        arguments.push_back(self);
    }
    if (rawArguments.is<sol::table>()) {
        const sol::table packed = rawArguments.as<sol::table>();
        for (std::size_t index = 1; index <= count; ++index) {
            arguments.push_back(packed.raw_get<sol::object>(index));
        }
    }
    return arguments;
}

std::vector<sol::object> callNodeGraphCallable(
    sol::state_view lua, const sol::object& callable, const sol::object& self,
    const sol::object& rawArguments) {
    if (!isRuntimeCallable(lua, callable)) {
        throw std::invalid_argument("Node graph value is not callable");
    }
    lua_State* state = lua.lua_state();
    const int base = lua_gettop(state);
    try {
        const std::vector<sol::object> arguments =
            nodeGraphCallArguments(self, rawArguments);
        const int count = invokeRuntimeFunction(
            state, callable, arguments, "node graph callable arguments");
        ensureRuntimeLuaStack(state, LUA_MINSTACK,
                              "node graph callable results");
        std::vector<sol::object> result;
        result.reserve(static_cast<std::size_t>(count));
        for (int index = 1; index <= count; ++index) {
            result.push_back(sol::stack::get<sol::object>(state, base + index));
        }
        lua_settop(state, base);
        return result;
    } catch (...) {
        lua_settop(state, base);
        throw;
    }
}

NodeResult runtimeNodeResult(const std::vector<sol::object>& values) {
    NodeResult result;
    result.count = values.size();
    result.values.reserve(values.size());
    for (const sol::object& value : values) {
        result.values.push_back(
            ludork::runtime::binding::readLuaValue<RuntimeValue>(value));
    }
    return result;
}

NodeIndex nodeGraphCacheKey(const sol::object& key) {
    if (key.is<std::string>()) {
        return key.as<std::string>();
    }
    if (key.get_type() != sol::type::number) {
        throw std::invalid_argument(
            "Node graph cache keys must be strings or integers");
    }
    lua_State* state = key.lua_state();
    key.push();
    if (lua_isinteger(state, -1) == 0) {
        lua_pop(state, 1);
        throw std::invalid_argument(
            "Node graph cache keys must be strings or integers");
    }
    const lua_Integer value = lua_tointeger(state, -1);
    lua_pop(state, 1);
    if (value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max()) {
        throw std::out_of_range("Node graph cache integer key is out of range");
    }
    return static_cast<int>(value);
}

sol::object nodeGraphCacheKeyObject(sol::state_view lua, const NodeIndex& key) {
    if (const int* index = std::get_if<int>(&key)) {
        return sol::make_object(lua, *index);
    }
    return sol::make_object(lua, std::get<std::string>(key));
}

NodeResult readPackedNodeResult(const sol::object& rawValue) {
    NodeResult result;
    if (rawValue.is<sol::table>()) {
        const sol::table values = rawValue.as<sol::table>();
        result.count = nodeGraphPackedCount(values);
        result.values.reserve(result.count);
        for (std::size_t index = 1; index <= result.count; ++index) {
            result.values.push_back(
                ludork::runtime::binding::readLuaValue<RuntimeValue>(
                    values.raw_get<sol::object>(index)));
        }
        return result;
    }
    result.count = 1;
    result.values.push_back(
        ludork::runtime::binding::readLuaValue<RuntimeValue>(rawValue));
    return result;
}

sol::table writePackedNodeResult(sol::state_view lua,
                                 const NodeResult& result) {
    if (result.count > static_cast<std::size_t>(INT_MAX)) {
        throw std::length_error("Node graph cache value count overflow");
    }
    sol::table values = lua.create_table(static_cast<int>(result.count), 1);
    values.raw_set("n", result.count);
    for (std::size_t index = 0; index < result.count; ++index) {
        if (index >= result.values.size() || result.values[index].isNil()) {
            continue;
        }
        values.raw_set(index + 1, ludork::runtime::binding::writeLuaValue(
                                      lua, result.values[index]));
    }
    return values;
}

sol::table nodeGraphObjectTable(const sol::object& value,
                                const char* description) {
    if (!value.is<sol::table>()) {
        throw std::invalid_argument(std::string(description) +
                                    " must be a table");
    }
    return value.as<sol::table>();
}

sol::table nodeGraphContextTarget(const sol::object& value,
                                  const char* description) {
    const sol::table table = nodeGraphObjectTable(value, description);
    const sol::object rawGraph = table.raw_get<sol::object>("__graph__");
    return rawGraph.is<sol::table>() ? rawGraph.as<sol::table>() : table;
}

}  // namespace

NodeGraphContextObjects createNodeGraphContext(sol::state_view lua,
                                               const sol::object& parentClass,
                                               const sol::object& parentValue) {
    sol::table context = lua.create_table();
    context[sol::metatable_key] = lua.create_table();
    sol::table graph = lua.create_table();
    graph.raw_set("parentClass", parentClass);
    graph.raw_set("localGraph", context);
    sol::table parent = createWeakTable(lua, "v");
    parent.raw_set(1, parentValue);
    sol::table graphMetatable = lua.create_table();
    graphMetatable.set_function(
        "__index",
        [parent](const sol::object&, const sol::object& key) -> sol::object {
            if (key.is<std::string>() && key.as<std::string>() == "parent") {
                return parent.raw_get<sol::object>(1);
            }
            return sol::make_object(parent.lua_state(), sol::lua_nil);
        });
    graphMetatable.set_function(
        "__newindex", [parent](sol::table target, const sol::object& key,
                               const sol::object& value) mutable {
            if (key.is<std::string>() && key.as<std::string>() == "parent") {
                parent.raw_set(1, value);
                return;
            }
            target.raw_set(key, value);
        });
    graph[sol::metatable_key] = graphMetatable;
    context.raw_set("__graph__", graph);
    registryTable(lua, NODEGRAPH_CONTEXTS_KEY, "k").raw_set(graph, context);
    return {sol::make_object(lua, context), sol::make_object(lua, graph)};
}

sol::object getNodeGraphContextValue(sol::state_view lua,
                                     const sol::object& context,
                                     const std::string& key) {
    static_cast<void>(lua);
    return nodeGraphObjectTable(context, "Node graph context")
        .raw_get<sol::object>(key);
}

void setNodeGraphContextValue(sol::state_view lua, const sol::object& context,
                              const std::string& key,
                              const sol::object& value) {
    static_cast<void>(lua);
    nodeGraphObjectTable(context, "Node graph context").raw_set(key, value);
}

sol::object getNodeGraphContextParent(sol::state_view lua,
                                      const sol::object& graph) {
    return protectedIndex(
        lua,
        sol::make_object(lua,
                         nodeGraphContextTarget(graph, "Node graph context")),
        sol::make_object(lua, std::string("parent")));
}

void setNodeGraphContextParent(sol::state_view lua, const sol::object& graph,
                               const sol::object& parent) {
    protectedAssign(lua,
                    sol::make_object(lua, nodeGraphContextTarget(
                                              graph, "Node graph context")),
                    sol::make_object(lua, std::string("parent")), parent);
}

std::shared_ptr<Node> createNodeGraphNode(
    sol::state_view lua, const sol::object& nodeModel, const sol::object& graph,
    const sol::object& parent, const std::string& nodeFunction,
    const sol::object& fallback, const sol::object& parameters) {
    if (!nodeModel.valid() || nodeModel.get_type() == sol::type::lua_nil) {
        return nullptr;
    }
    const sol::object rawConstructor =
        ludork::standard::class_runtime::protectedGet(
            lua, nodeModel, sol::make_object(lua, "new"));
    if (!rawConstructor.is<sol::protected_function>()) {
        return nullptr;
    }
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    graph.push();
    parent.push();
    sol::make_object(lua, nodeFunction).push();
    fallback.push();
    parameters.push();
    try {
        const int resultCount =
            ludork::standard::class_runtime::invoke(state, rawConstructor, 5);
        if (resultCount == 0 || lua_isnil(state, stackBase + 1) != 0) {
            lua_settop(state, stackBase);
            return nullptr;
        }
        const sol::object result =
            sol::stack::get<sol::object>(state, stackBase + 1);
        std::shared_ptr<Node> node;
        try {
            node =
                ludork::runtime::binding::readLuaValue<std::shared_ptr<Node>>(
                    result);
        } catch (const std::exception& error) {
            throw std::runtime_error(
                std::string("Node model constructor must return an Engine.Node "
                            "or nil: ") +
                error.what());
        }
        lua_settop(state, stackBase);
        return node;
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

NodeResult invokeNodeGraphCallable(sol::state_view lua,
                                   const sol::object& callable,
                                   const sol::object& self,
                                   const sol::object& arguments,
                                   const sol::object& rawContext) {
    if (!isRuntimeCallable(lua, callable)) {
        throw std::runtime_error("Node graph value is not callable");
    }
    const std::vector<sol::object> callableArguments =
        nodeGraphCallArguments(self, arguments);
    sol::table refLocals = registryTable(lua, NODEGRAPH_REF_LOCALS_KEY, "k");
    sol::object oldActive = nilObject(lua);
    const sol::object oldRefLocal = refLocals.raw_get<sol::object>(callable);
    const bool hadRefLocal =
        oldRefLocal.valid() && oldRefLocal.get_type() != sol::type::lua_nil;
    bool hadActive = false;
    sol::table context = lua.create_table();
    if (rawContext.is<sol::table>()) {
        context = rawContext.as<sol::table>();
        oldActive = context.raw_get<sol::object>("__activeNodeFunction__");
        hadActive =
            oldActive.valid() && oldActive.get_type() != sol::type::lua_nil;
        context.raw_set("__activeNodeFunction__", callable);
        refLocals.raw_set(callable, context);
    }

    lua_State* state = lua.lua_state();
    const int base = lua_gettop(state);
    std::vector<sol::object> results;
    std::exception_ptr failure;
    try {
        const int count =
            invokeRuntimeFunction(state, callable, callableArguments,
                                  "node graph runtime callable arguments");
        ensureRuntimeLuaStack(state, LUA_MINSTACK,
                              "node graph runtime callable results");
        results.reserve(static_cast<std::size_t>(count));
        for (int index = 1; index <= count; ++index) {
            results.push_back(
                sol::stack::get<sol::object>(state, base + index));
        }
        lua_settop(state, base);
    } catch (...) {
        lua_settop(state, base);
        failure = std::current_exception();
    }

    if (rawContext.is<sol::table>()) {
        context.raw_set("__activeNodeFunction__",
                        hadActive ? oldActive : sol::lua_nil);
        refLocals.raw_set(callable, hadRefLocal ? oldRefLocal : sol::lua_nil);
    }
    if (failure != nullptr) {
        std::rethrow_exception(failure);
    }
    return runtimeNodeResult(results);
}

sol::object nodeGraphRefLocal(sol::state_view lua,
                              const sol::object& callable) {
    return registryTable(lua, NODEGRAPH_REF_LOCALS_KEY, "k")
        .raw_get<sol::object>(callable);
}

NodeGraphConditionResult evaluateNodeGraphCondition(
    sol::state_view lua, const sol::object& condition) {
    const std::vector<sol::object> rawResults =
        callNodeGraphCallable(lua, condition, nilObject(lua), nilObject(lua));
    NodeGraphConditionResult result;
    if (!rawResults.empty() &&
        rawResults.front().get_type() != sol::type::lua_nil) {
        const sol::object first = rawResults.front();
        if (first.is<sol::table>()) {
            const sol::table tableValue = first.as<sol::table>();
            const sol::object rawCount = tableValue.raw_get<sol::object>("n");
            if (rawCount.is<std::size_t>()) {
                result.result.count = rawCount.as<std::size_t>();
                result.result.values.reserve(result.result.count);
                for (std::size_t index = 1; index <= result.result.count;
                     ++index) {
                    result.result.values.push_back(
                        ludork::runtime::binding::readLuaValue<RuntimeValue>(
                            tableValue.raw_get<sol::object>(index)));
                }
            } else if (tableValue.size() > 0) {
                result.result.count = tableValue.size();
                result.result.values.reserve(result.result.count);
                for (std::size_t index = 1; index <= result.result.count;
                     ++index) {
                    result.result.values.push_back(
                        ludork::runtime::binding::readLuaValue<RuntimeValue>(
                            tableValue.raw_get<sol::object>(index)));
                }
            } else {
                for (const auto& entry : tableValue) {
                    result.result.values.push_back(
                        ludork::runtime::binding::readLuaValue<RuntimeValue>(
                            entry.second));
                }
                result.result.count = result.result.values.size();
            }
        } else {
            result.result.values.push_back(
                ludork::runtime::binding::readLuaValue<RuntimeValue>(first));
            result.result.count = 1;
        }
    }
    if (condition.is<sol::table>()) {
        const sol::object rawIsFinished =
            condition.as<sol::table>().get<sol::object>("isFinished");
        if (isRuntimeCallable(lua, rawIsFinished)) {
            const std::vector<sol::object> finished = callNodeGraphCallable(
                lua, rawIsFinished, condition, nilObject(lua));
            if (finished.size() != 1 || !finished.front().is<bool>()) {
                throw std::runtime_error(
                    "Node graph condition isFinished must return exactly one "
                    "boolean");
            }
            result.finished = luaBoolean(finished.front());
        }
    }
    return result;
}

NodeCache readNodeGraphCache(sol::state_view lua, const sol::object& rawCache) {
    static_cast<void>(lua);
    NodeCache result;
    if (!rawCache.is<sol::table>()) {
        return result;
    }
    for (const auto& entry : rawCache.as<sol::table>()) {
        result.emplace(nodeGraphCacheKey(entry.first),
                       readPackedNodeResult(entry.second));
    }
    return result;
}

void writeNodeGraphCache(sol::state_view lua, const sol::object& rawCache,
                         const NodeCache& values) {
    if (!rawCache.is<sol::table>()) {
        return;
    }
    sol::table cache = rawCache.as<sol::table>();
    std::vector<sol::object> existingKeys;
    existingKeys.reserve(cache.size());
    for (const auto& entry : cache) {
        existingKeys.push_back(entry.first);
    }
    for (const sol::object& key : existingKeys) {
        cache.raw_set(key, sol::lua_nil);
    }
    for (const auto& [key, result] : values) {
        cache.raw_set(nodeGraphCacheKeyObject(lua, key),
                      writePackedNodeResult(lua, result));
    }
}

void clearNodeGraphRuntimeCaches(sol::state_view lua) {
    lua.registry().raw_set(NODEGRAPH_REF_LOCALS_KEY, sol::lua_nil);
    lua.registry().raw_set(NODEGRAPH_CONTEXTS_KEY, sol::lua_nil);
}

}  // namespace ludork::engine::runtime_detail
