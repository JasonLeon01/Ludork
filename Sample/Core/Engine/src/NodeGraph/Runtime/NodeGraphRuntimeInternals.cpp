#include <NodeGraph/Runtime/NodeGraphRuntimeInternal.hpp>
#include <NodeGraph/Graph.hpp>
#include <NodeGraph/Node.hpp>
#include <Runtime/RuntimeReference.hpp>

#include <climits>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ludork::engine::runtime_detail {
using namespace ludork::runtime::reference;
namespace {

constexpr const char* NODEGRAPH_REF_LOCALS_KEY =
    "Ludork.Engine.NodeGraph.refLocals";
constexpr const char* NODEGRAPH_CONTEXTS_KEY =
    "Ludork.Engine.NodeGraph.contexts";

std::size_t packedCount(const RuntimeValue& values) {
    const RuntimeValue count = rawGet(values, "n");
    return is<std::size_t>(count) ? as<std::size_t>(count) : length(values);
}

RuntimeValue::Array callArguments(const RuntimeValue& self,
                                  const RuntimeValue& arguments) {
    const std::size_t count = isTable(arguments) ? packedCount(arguments) : 0;
    const std::size_t selfCount = self.isNil() ? 0 : 1;
    if (count > static_cast<std::size_t>(INT_MAX) - selfCount) {
        throw std::length_error("Node graph argument count overflow");
    }
    RuntimeValue::Array result;
    result.reserve(count + selfCount);
    if (selfCount != 0) {
        result.push_back(self);
    }
    for (std::size_t index = 1; index <= count; ++index) {
        result.push_back(rawGet(arguments, index));
    }
    return result;
}

NodeResult nodeResult(const RuntimeValue::Array& values) {
    NodeResult result;
    result.count = values.size();
    result.values.reserve(values.size());
    for (const RuntimeValue& value : values) {
        result.values.push_back(data(value));
    }
    return result;
}

RuntimeValue contextTarget(const RuntimeValue& value) {
    requireTable(value);
    const RuntimeValue graph = rawGet(value, "__graph__");
    return isTable(graph) ? graph : value;
}

NodeIndex cacheKey(const RuntimeValue& value) {
    if (is<std::string>(value)) {
        return as<std::string>(value);
    }
    if (!is<std::int64_t>(value)) {
        throw std::invalid_argument(
            "Node graph cache keys must be strings or integers");
    }
    if (!is<int>(value)) {
        throw std::out_of_range("Node graph cache integer key is out of range");
    }
    return as<int>(value);
}

RuntimeValue cacheKeyValue(const NodeIndex& key) {
    if (const int* index = std::get_if<int>(&key)) {
        return makeValue(*index);
    }
    return makeValue(std::get<std::string>(key));
}

}  // namespace

NodeGraphContextObjects createNodeGraphContext(
    const RuntimeValue& parentClass, const RuntimeValue& parentValue) {
    RuntimeValue context = table();
    setMetatable(context, table());
    RuntimeValue graph = table();
    rawSet(graph, "parentClass", parentClass);
    rawSet(graph, "localGraph", context);
    RuntimeValue parent = table(WeakMode::Values);
    rawSet(parent, 1, parentValue);
    RuntimeValue graphMetatable = table();
    rawSet(graphMetatable, "__index",
           callback([parent](const RuntimeValue::Array& arguments)
                        -> RuntimeValue::Array {
               if (arguments.size() != 2) {
                   throw std::invalid_argument(
                       "Graph context index expects two arguments");
               }
               return {is<std::string>(arguments[1]) &&
                               as<std::string>(arguments[1]) == "parent"
                           ? rawGet(parent, 1)
                           : RuntimeValue()};
           }));
    rawSet(graphMetatable, "__newindex",
           callback([parent](const RuntimeValue::Array& arguments)
                        -> RuntimeValue::Array {
               if (arguments.size() != 3) {
                   throw std::invalid_argument(
                       "Graph context assignment expects three arguments");
               }
               if (is<std::string>(arguments[1]) &&
                   as<std::string>(arguments[1]) == "parent") {
                   rawSet(parent, 1, arguments[2]);
               } else {
                   rawSet(arguments[0], arguments[1], arguments[2]);
               }
               return {};
           }));
    setMetatable(graph, graphMetatable);
    rawSet(context, "__graph__", graph);
    rawSet(registryTable(NODEGRAPH_CONTEXTS_KEY, WeakMode::Keys), graph,
           context);
    return {context, graph};
}

RuntimeValue getNodeGraphContextValue(const RuntimeValue& context,
                                      const std::string& key) {
    return rawGet(requireTable(context), key);
}
void setNodeGraphContextValue(const RuntimeValue& context,
                              const std::string& key,
                              const RuntimeValue& value) {
    rawSet(requireTable(context), key, value);
}
RuntimeValue getNodeGraphContextParent(const RuntimeValue& graph) {
    return get(contextTarget(graph), "parent");
}
void setNodeGraphContextParent(const RuntimeValue& graph,
                               const RuntimeValue& parent) {
    set(contextTarget(graph), "parent", parent);
}

std::shared_ptr<Node> createNodeGraphNode(const RuntimeValue& nodeModel,
                                          const RuntimeValue& graph,
                                          const RuntimeValue& parent,
                                          const std::string& nodeFunction,
                                          const RuntimeValue& fallback,
                                          const RuntimeValue& parameters) {
    if (nodeModel.isNil()) {
        return nullptr;
    }
    const RuntimeValue constructor = get(nodeModel, "new");
    if (!isFunction(constructor)) {
        return nullptr;
    }
    const RuntimeValue result =
        first(invoke(constructor, {graph, parent, makeValue(nodeFunction),
                                   fallback, parameters}));
    if (result.isNil()) {
        return nullptr;
    }
    const std::shared_ptr<Node> node =
        std::dynamic_pointer_cast<Node>(object(result));
    if (node == nullptr) {
        throw std::runtime_error(
            "Node model constructor must return an Engine.Node or nil");
    }
    return node;
}

NodeResult invokeNodeGraphCallable(const RuntimeValue& callable,
                                   const RuntimeValue& self,
                                   const RuntimeValue& arguments,
                                   const RuntimeValue& context) {
    if (!isCallable(callable)) {
        throw std::runtime_error("Node graph value is not callable");
    }
    const RuntimeValue::Array values = callArguments(self, arguments);
    const RuntimeValue refLocals =
        registryTable(NODEGRAPH_REF_LOCALS_KEY, WeakMode::Keys);
    const RuntimeValue oldRef = rawGet(refLocals, callable);
    RuntimeValue oldActive;
    const bool hasContext = isTable(context);
    if (hasContext) {
        oldActive = rawGet(context, "__activeNodeFunction__");
        rawSet(context, "__activeNodeFunction__", callable);
        rawSet(refLocals, callable, context);
    }
    RuntimeValue::Array results;
    std::exception_ptr failure;
    try {
        results = invoke(callable, values);
    } catch (...) {
        failure = std::current_exception();
    }
    if (hasContext) {
        rawSet(context, "__activeNodeFunction__", oldActive);
        rawSet(refLocals, callable, oldRef);
    }
    if (failure != nullptr) {
        std::rethrow_exception(failure);
    }
    return nodeResult(results);
}

RuntimeValue nodeGraphRefLocal(const RuntimeValue& callable) {
    return rawGet(registryTable(NODEGRAPH_REF_LOCALS_KEY, WeakMode::Keys),
                  callable);
}

NodeGraphConditionResult evaluateNodeGraphCondition(
    const RuntimeValue& condition) {
    const RuntimeValue firstResult = first(invoke(condition));
    NodeGraphConditionResult result;
    if (!firstResult.isNil()) {
        RuntimeValue::Array values;
        if (isTable(firstResult)) {
            const RuntimeValue count = rawGet(firstResult, "n");
            if (is<std::size_t>(count) || length(firstResult) > 0) {
                const std::size_t size = is<std::size_t>(count)
                                             ? as<std::size_t>(count)
                                             : length(firstResult);
                values.reserve(size);
                for (std::size_t index = 1; index <= size; ++index) {
                    values.push_back(rawGet(firstResult, index));
                }
            } else {
                for (const auto& entry : entries(firstResult)) {
                    values.push_back(entry.second);
                }
            }
        } else {
            values.push_back(firstResult);
        }
        result.result = nodeResult(values);
    }
    if (isTable(condition)) {
        const RuntimeValue finishedFunction = get(condition, "isFinished");
        if (isCallable(finishedFunction)) {
            const RuntimeValue::Array finished =
                invoke(finishedFunction, {condition});
            if (finished.size() != 1 || !is<bool>(finished.front())) {
                throw std::runtime_error(
                    "Node graph condition isFinished must return exactly one "
                    "boolean");
            }
            result.finished = boolean(finished.front());
        }
    }
    return result;
}

NodeCache readNodeGraphCache(const RuntimeValue& cache) {
    NodeCache result;
    if (!isTable(cache)) {
        return result;
    }
    for (const auto& entry : entries(cache)) {
        RuntimeValue::Array values;
        if (isTable(entry.second)) {
            const std::size_t count = packedCount(entry.second);
            values.reserve(count);
            for (std::size_t index = 1; index <= count; ++index) {
                values.push_back(rawGet(entry.second, index));
            }
        } else {
            values.push_back(entry.second);
        }
        result.emplace(cacheKey(entry.first), nodeResult(values));
    }
    return result;
}

void writeNodeGraphCache(const RuntimeValue& cache, const NodeCache& values) {
    if (!isTable(cache)) {
        return;
    }
    for (const auto& entry : entries(cache)) {
        rawSet(cache, entry.first, RuntimeValue());
    }
    for (const auto& [key, result] : values) {
        if (result.count > static_cast<std::size_t>(INT_MAX)) {
            throw std::length_error("Node graph cache value count overflow");
        }
        RuntimeValue packed = table();
        rawSet(packed, "n", result.count);
        for (std::size_t index = 0; index < result.count; ++index) {
            if (index < result.values.size() && !result.values[index].isNil()) {
                rawSet(packed, index + 1, result.values[index]);
            }
        }
        rawSet(cache, cacheKeyValue(key), packed);
    }
}

void clearNodeGraphRuntimeCaches() {
    rawSet(registry(), NODEGRAPH_REF_LOCALS_KEY, RuntimeValue());
    rawSet(registry(), NODEGRAPH_CONTEXTS_KEY, RuntimeValue());
}

}  // namespace ludork::engine::runtime_detail
