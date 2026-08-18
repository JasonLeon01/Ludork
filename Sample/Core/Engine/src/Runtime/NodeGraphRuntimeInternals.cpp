#include "RuntimeSubsystemServices.hpp"

#include <ClassServices.hpp>
#include <LuaError.hpp>

extern "C" {
#include <lua.h>
}

#include <climits>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::engine::runtime_detail {
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

sol::table packedRuntimeObjects(sol::state_view lua,
                                const std::vector<sol::object>& values) {
    sol::table result = lua.create_table(static_cast<int>(values.size()), 1);
    result.raw_set("n", values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index].valid() &&
            values[index].get_type() != sol::type::lua_nil) {
            result.raw_set(index + 1, values[index]);
        }
    }
    return result;
}

std::size_t nodeGraphPackedCount(const sol::table& values);

std::vector<sol::object> nodeGraphCallArguments(
    const sol::object& self, const sol::object& rawArguments) {
    const std::size_t selfCount =
        self.valid() && self.get_type() != sol::type::lua_nil ? 1 : 0;
    const std::size_t count = rawArguments.is<sol::table>()
                                  ? nodeGraphPackedCount(
                                        rawArguments.as<sol::table>())
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
            result.push_back(
                sol::stack::get<sol::object>(state, base + index));
        }
        lua_settop(state, base);
        return result;
    } catch (...) {
        lua_settop(state, base);
        throw;
    }
}

std::size_t nodeGraphPackedCount(const sol::table& values) {
    const sol::object rawCount = values.raw_get<sol::object>("n");
    return rawCount.is<std::size_t>() ? rawCount.as<std::size_t>()
                                      : values.size();
}

sol::table copyNodeGraphPackedValues(sol::state_view lua,
                                     const sol::object& rawValues,
                                     std::size_t count) {
    sol::table values = lua.create_table(static_cast<int>(count), 1);
    values.raw_set("n", count);
    if (rawValues.is<sol::table>()) {
        const sol::table source = rawValues.as<sol::table>();
        for (std::size_t index = 1; index <= count; ++index) {
            const sol::object value = source.raw_get<sol::object>(index);
            if (value.valid() && value.get_type() != sol::type::lua_nil) {
                values.raw_set(index, value);
            }
        }
    } else if (count > 0 && rawValues.valid() &&
               rawValues.get_type() != sol::type::lua_nil) {
        values.raw_set(1, rawValues);
    }
    return values;
}

bool isNodeGraphCacheKey(const sol::object& key) {
    if (key.is<std::string>()) {
        return true;
    }
    if (key.get_type() != sol::type::number) {
        return false;
    }
    lua_State* state = key.lua_state();
    key.push();
    const bool integer = lua_isinteger(state, -1) != 0;
    lua_pop(state, 1);
    return integer;
}

int decodeNodeGraphCache(sol::state_view lua, const sol::object& rawCache) {
    sol::table descriptors = lua.create_table();
    std::size_t descriptorIndex = 1;
    if (rawCache.is<sol::table>()) {
        for (const auto& entry : rawCache.as<sol::table>()) {
            if (!isNodeGraphCacheKey(entry.first)) {
                throw std::invalid_argument(
                    "Node graph cache keys must be strings or integers");
            }
            std::size_t count = 1;
            if (entry.second.is<sol::table>()) {
                count = nodeGraphPackedCount(entry.second.as<sol::table>());
            }
            sol::table descriptor = lua.create_table();
            descriptor.raw_set("key", entry.first);
            descriptor.raw_set(
                "values", copyNodeGraphPackedValues(lua, entry.second, count));
            descriptor.raw_set("count", count);
            descriptors.raw_set(descriptorIndex++, descriptor);
        }
    }
    descriptors.raw_set("n", descriptorIndex - 1);
    return runtimeResolverResult(lua, {sol::make_object(lua, descriptors)});
}

int encodeNodeGraphCache(sol::state_view lua, const sol::object& rawCache,
                         const sol::object& rawDescriptors) {
    if (!rawCache.is<sol::table>()) {
        return runtimeResolverResult(lua, {});
    }
    sol::table cache = rawCache.as<sol::table>();
    std::vector<sol::object> existingKeys;
    for (const auto& entry : cache) {
        existingKeys.push_back(entry.first);
    }
    for (const sol::object& key : existingKeys) {
        cache.raw_set(key, sol::lua_nil);
    }

    if (!rawDescriptors.is<sol::table>()) {
        return runtimeResolverResult(lua, {});
    }
    const sol::table descriptors = rawDescriptors.as<sol::table>();
    const std::size_t descriptorCount = nodeGraphPackedCount(descriptors);
    for (std::size_t index = 1; index <= descriptorCount; ++index) {
        const sol::object rawDescriptor =
            descriptors.raw_get<sol::object>(index);
        if (!rawDescriptor.is<sol::table>()) {
            throw std::invalid_argument(
                "Node graph cache descriptor must be a table");
        }
        const sol::table descriptor = rawDescriptor.as<sol::table>();
        const sol::object key = descriptor.raw_get<sol::object>("key");
        if (!isNodeGraphCacheKey(key)) {
            throw std::invalid_argument(
                "Node graph cache descriptor key must be a string or integer");
        }
        const sol::object rawValues = descriptor.raw_get<sol::object>("values");
        const sol::object rawCount = descriptor.raw_get<sol::object>("count");
        const std::size_t count =
            rawCount.is<std::size_t>() ? rawCount.as<std::size_t>()
            : rawValues.is<sol::table>()
                ? nodeGraphPackedCount(rawValues.as<sol::table>())
                : 0;
        cache.raw_set(key, copyNodeGraphPackedValues(lua, rawValues, count));
    }
    return runtimeResolverResult(lua, {});
}

}  // namespace

int nodeGraphContext(sol::state_view lua, const RuntimeArguments& arguments) {
    const sol::object first = runtimeResolverArgument(lua, arguments, 1);
    const sol::object second = runtimeResolverArgument(lua, arguments, 2);
    const sol::object third = runtimeResolverArgument(lua, arguments, 3);
    if (first.is<sol::table>() && second.is<std::string>()) {
        sol::table context = first.as<sol::table>();
        const std::string operation = second.as<std::string>();
        if (operation == "get") {
            const sol::object value =
                third.is<std::string>()
                    ? context.raw_get<sol::object>(third.as<std::string>())
                    : nilObject(lua);
            return runtimeResolverResult(lua, {value});
        }
        if (operation == "set") {
            if (!third.is<std::string>()) {
                throw std::invalid_argument(
                    "Node graph context key must be a string");
            }
            context.raw_set(third.as<std::string>(),
                            runtimeResolverArgument(lua, arguments, 4));
            return runtimeResolverResult(lua, {});
        }
        const sol::object rawGraph = context.raw_get<sol::object>("__graph__");
        const sol::object graph = rawGraph.is<sol::table>() ? rawGraph : first;
        if (operation == "getParent") {
            return runtimeResolverResult(
                lua,
                {protectedIndex(lua, graph,
                                sol::make_object(lua, std::string("parent")))});
        }
        if (operation == "setParent") {
            protectedAssign(lua, graph,
                            sol::make_object(lua, std::string("parent")),
                            third);
            return runtimeResolverResult(lua, {});
        }
    }
    sol::table context = lua.create_table();
    context[sol::metatable_key] = lua.create_table();
    sol::table graph = lua.create_table();
    graph.raw_set("parentClass", first);
    graph.raw_set("localGraph", context);
    sol::table parent = createWeakTable(lua, "v");
    parent.raw_set(1, second);
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
    return runtimeResolverResult(
        lua, {sol::make_object(lua, context), sol::make_object(lua, graph)});
}

int invokeNodeGraphCallable(sol::state_view lua,
                            const RuntimeArguments& arguments) {
    const sol::object callable = runtimeResolverArgument(lua, arguments, 1);
    const sol::object self = runtimeResolverArgument(lua, arguments, 2);
    const sol::object callArguments =
        runtimeResolverArgument(lua, arguments, 3);
    const sol::object rawContext = runtimeResolverArgument(lua, arguments, 4);
    if (!isRuntimeCallable(lua, callable)) {
        throw std::runtime_error("Node graph value is not callable");
    }
    const std::vector<sol::object> callableArguments =
        nodeGraphCallArguments(self, callArguments);
    sol::table refLocals = registryTable(lua, NODEGRAPH_REF_LOCALS_KEY, "k");
    sol::object oldActive = nilObject(lua);
    sol::object oldRefLocal = refLocals.raw_get<sol::object>(callable);
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
        const int count = invokeRuntimeFunction(
            state, callable, callableArguments,
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
        if (hadActive) {
            context.raw_set("__activeNodeFunction__", oldActive);
        } else {
            context.raw_set("__activeNodeFunction__", sol::lua_nil);
        }
        if (hadRefLocal) {
            refLocals.raw_set(callable, oldRefLocal);
        } else {
            refLocals.raw_set(callable, sol::lua_nil);
        }
    }
    if (failure != nullptr) {
        std::rethrow_exception(failure);
    }

    sol::table descriptor = lua.create_table();
    descriptor.raw_set("values", packedRuntimeObjects(lua, results));
    descriptor.raw_set("count", results.size());
    return runtimeResolverResult(lua, {sol::make_object(lua, descriptor)});
}

int createNodeGraphNode(sol::state_view lua,
                        const RuntimeArguments& arguments) {
    const sol::object nodeModel = runtimeResolverArgument(lua, arguments, 1);
    if (nodeModel.get_type() == sol::type::lua_nil) {
        return runtimeResolverResult(lua, {});
    }
    const sol::object rawConstructor =
        ludork::standard::class_runtime::protectedGet(
            lua, nodeModel, sol::make_object(lua, "new"));
    if (!rawConstructor.is<sol::protected_function>()) {
        return runtimeResolverResult(lua, {});
    }
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    for (std::size_t index = 1; index <= 5; ++index) {
        runtimeResolverArgument(lua, arguments, index + 1).push();
    }
    try {
        const int resultCount = ludork::standard::class_runtime::invoke(
            state, rawConstructor, 5);
        if (resultCount == 0) {
            lua_pushnil(state);
        } else {
            lua_settop(state, stackBase + 1);
        }
        return 1;
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

int bridgeNodeGraphCache(sol::state_view lua,
                         const RuntimeArguments& arguments) {
    const sol::object rawOperation = runtimeResolverArgument(lua, arguments, 1);
    if (!rawOperation.is<std::string>()) {
        throw std::invalid_argument(
            "Node graph cache operation must be a string");
    }
    const std::string operation = rawOperation.as<std::string>();
    if (operation == "decode") {
        return decodeNodeGraphCache(lua,
                                    runtimeResolverArgument(lua, arguments, 2));
    }
    if (operation == "encode") {
        return encodeNodeGraphCache(lua,
                                    runtimeResolverArgument(lua, arguments, 2),
                                    runtimeResolverArgument(lua, arguments, 3));
    }
    throw std::invalid_argument("Unknown node graph cache operation: " +
                                operation);
}

int evaluateNodeGraphCondition(sol::state_view lua,
                               const RuntimeArguments& arguments) {
    const sol::object condition = runtimeResolverArgument(lua, arguments, 1);
    const std::vector<sol::object> rawResults =
        callNodeGraphCallable(lua, condition, nilObject(lua), nilObject(lua));
    std::vector<sol::object> values;
    if (!rawResults.empty() &&
        rawResults.front().get_type() != sol::type::lua_nil) {
        const sol::object first = rawResults.front();
        if (first.is<sol::table>()) {
            const sol::table tableValue = first.as<sol::table>();
            const sol::object rawCount = tableValue.raw_get<sol::object>("n");
            if (rawCount.is<std::size_t>()) {
                const std::size_t count = rawCount.as<std::size_t>();
                values.reserve(count);
                for (std::size_t index = 1; index <= count; ++index) {
                    values.push_back(tableValue.raw_get<sol::object>(index));
                }
            } else {
                const std::size_t count = tableValue.size();
                if (count > 0) {
                    values.reserve(count);
                    for (std::size_t index = 1; index <= count; ++index) {
                        values.push_back(
                            tableValue.raw_get<sol::object>(index));
                    }
                } else {
                    for (const auto& entry : tableValue) {
                        values.push_back(entry.second);
                    }
                }
            }
        } else {
            values.push_back(first);
        }
    }
    bool finished = true;
    if (condition.is<sol::table>()) {
        const sol::object rawIsFinished =
            condition.as<sol::table>().get<sol::object>("isFinished");
        if (isRuntimeCallable(lua, rawIsFinished)) {
            const std::vector<sol::object> result = callNodeGraphCallable(
                lua, rawIsFinished, condition, nilObject(lua));
            finished = !result.empty() && luaBoolean(result.front());
        }
    }
    sol::table descriptor = lua.create_table();
    descriptor.raw_set("values", packedRuntimeObjects(lua, values));
    descriptor.raw_set("count", values.size());
    descriptor.raw_set("finished", finished);
    return runtimeResolverResult(lua, {sol::make_object(lua, descriptor)});
}

void clearNodeGraphRuntimeCaches(sol::state_view lua) {
    lua.registry().raw_set(NODEGRAPH_REF_LOCALS_KEY, sol::lua_nil);
    lua.registry().raw_set(NODEGRAPH_CONTEXTS_KEY, sol::lua_nil);
}

}  // namespace ludork::engine::runtime_detail
