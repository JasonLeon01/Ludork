#include <Runtime/RuntimeReference.hpp>
#include "BlueprintRuntimeInternal.hpp"
#include <EngineRuntimeServices.hpp>

#include <Gameplay/Components/ComponentRuntime.hpp>
#include <Gameplay/EngineClassRuntime/EngineClassRuntimeInternal.hpp>
#include <NodeGraph/Graph.hpp>
#include <Gameplay/EngineClassRuntime.hpp>
#include <NodeGraph/Runtime/NodeGraphRuntimeInternal.hpp>
#include <RuntimeSession.hpp>
#include <Utf8Path.hpp>
#include <Utils/DataValue.hpp>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::engine::runtime_detail {

using namespace ludork::runtime::reference;

std::shared_ptr<Graph> requireBlueprintGraph(const RuntimeValue& graph) {
    const std::shared_ptr<Graph> nativeGraph =
        kind(graph) == "userdata"
            ? std::dynamic_pointer_cast<Graph>(
                  ludork::runtime::reference::object(graph))
            : nullptr;
    if (nativeGraph == nullptr) {
        throw std::invalid_argument("Blueprint graph must be an Engine.Graph");
    }
    return nativeGraph;
}

RuntimeValue blueprintEngineType(const char* name) {
    const RuntimeValue rawEngine = rawGet(globals(), "Engine");
    if (!isTable(rawEngine)) {
        throw std::runtime_error("Engine module is not initialized");
    }
    return rawGet(rawEngine, name);
}

bool blueprintIsInstance(const RuntimeValue& value, const RuntimeValue& type) {
    return isTable(type) && isInstance(value, type);
}

RuntimeValue callRuntimeMethodFirst(
    const RuntimeValue& object, const char* name,
    const std::vector<RuntimeValue>& arguments) {
    const RuntimeValue method = get(object, retain(makeValue(name)));
    if (!isFunction(method)) {
        return RuntimeValue();
    }
    std::vector<RuntimeValue> values;
    values.reserve(arguments.size() + 1);
    values.push_back(object);
    values.insert(values.end(), arguments.begin(), arguments.end());
    return first(invoke(method, values));
}

std::optional<double> runtimeNumber(const RuntimeValue& value) {
    const RuntimeValue rawToNumber = rawGet(globals(), "tonumber");
    if (!isFunction(rawToNumber)) {
        return std::nullopt;
    }
    RuntimeValue toNumber = rawToNumber;
    RuntimeValue::Array converted = invoke(toNumber, {makeValue(value)});
    const RuntimeValue result = first(converted);
    return is<double>(result) ? std::optional<double>(as<double>(result))
                              : std::nullopt;
}

bool blueprintGraphHasExecutableEvent(const RuntimeValue& graph,
                                      const std::string& eventName) {
    if (graph.isNil()) {
        return false;
    }
    return requireBlueprintGraph(graph)->hasExecutableEvent(eventName);
}

bool blueprintGraphDataHasExecutableEvent(const RuntimeValue& graphData,
                                          const std::string& eventName) {
    if (!isTable(graphData)) {
        return false;
    }
    const RuntimeValue data = graphData;
    const RuntimeValue nodeGraph = get(data, "nodeGraph");
    const RuntimeValue startNodes = get(data, "startNodes");
    if (!isTable(nodeGraph) || !isTable(startNodes)) {
        return false;
    }
    const RuntimeValue eventGraph = get(nodeGraph, eventName);
    const RuntimeValue rawStart = get(startNodes, eventName);
    if (!isTable(eventGraph) || rawStart.isNil()) {
        return false;
    }
    const RuntimeValue nodes = get(eventGraph, "nodes");
    const std::optional<double> start = runtimeNumber(rawStart);
    return isTable(nodes) && start.has_value() && *start >= 0.0 &&
           *start < static_cast<double>(length(nodes));
}

bool generatedBlueprintGraphHasExecutableEvent(const RuntimeValue& classType,
                                               const std::string& eventName) {
    const RuntimeValue rawScriptMixin = get(classType, "scriptMixin");
    if (is<bool>(rawScriptMixin) && as<bool>(rawScriptMixin)) {
        return false;
    }
    const RuntimeValue rawPath = rawGet(classType, "__blueprintClassPath");
    if (!is<std::string>(rawPath)) {
        return false;
    }
    return ludork::engine::class_runtime_detail::classGraphHasExecutableEvent(
        as<std::string>(rawPath), eventName);
}

RuntimeValue generatedBlueprintGraph(const RuntimeValue& object,
                                     const RuntimeValue& classType) {
    RuntimeValue rawCache = get(object, retain(makeValue("_parentGraphs")));
    RuntimeValue cache = isTable(rawCache) ? rawCache : table();
    if (!isTable(rawCache)) {
        set(object, retain(makeValue("_parentGraphs")),
            retain(makeValue(cache)));
    }
    RuntimeValue graph = rawGet(cache, classType);
    if (graph.isNil()) {
        const RuntimeValue rawPath = rawGet(classType, "__blueprintClassPath");
        if (!is<std::string>(rawPath)) {
            return RuntimeValue();
        }
        graph = ludork::engine::class_runtime_detail::instantiateClassGraph(
            as<std::string>(rawPath), object);
        if (!graph.isNil()) {
            rawSet(cache, classType, graph);
        }
    }
    return graph;
}

RuntimeValue blueprintEventKeywordArguments(
    const RuntimeValue& classType, const std::string& eventName,
    const RuntimeValue& rawArguments, const RuntimeValue& rawKeywordArguments) {
    RuntimeValue result = table();
    if (isTable(rawKeywordArguments)) {
        for (const auto& entry : entries(rawKeywordArguments)) {
            rawSet(result, entry.first, entry.second);
        }
    }
    if (!isTable(rawArguments) || length(rawArguments) == 0) {
        return result;
    }
    const RuntimeValue method = get(classType, eventName);
    if (!isFunction(method)) {
        return result;
    }
    const RuntimeValue names = runtimeDescriptorParameters(
        runtimeEventDescriptor(method, classType, eventName));
    const RuntimeValue arguments = rawArguments;
    const std::size_t count = std::min(length(arguments), length(names));
    for (std::size_t index = 1; index <= count; ++index) {
        const RuntimeValue rawName = rawGet(names, index);
        if (!is<std::string>(rawName)) {
            continue;
        }
        const std::string name = as<std::string>(rawName);
        if (kind(get(result, name)) == "nil") {
            set(result, name, get(arguments, index));
        }
    }
    return result;
}

void mergeBlueprintLocalArguments(const RuntimeValue& classType,
                                  const std::string& eventName,
                                  RuntimeValue keywordArguments,
                                  const RuntimeValue& localGraph) {
    if (!isTable(localGraph)) {
        return;
    }
    const RuntimeValue method = get(classType, eventName);
    if (!isFunction(method)) {
        return;
    }
    const RuntimeValue names = runtimeDescriptorParameters(
        runtimeEventDescriptor(method, classType, eventName));
    for (std::size_t index = 1; index <= length(names); ++index) {
        const RuntimeValue rawName = rawGet(names, index);
        if (!is<std::string>(rawName)) {
            continue;
        }
        const std::string name = as<std::string>(rawName);
        if (kind(get(keywordArguments, name)) != "nil") {
            continue;
        }
        const RuntimeValue value = get(localGraph, "__" + name + "__");
        if (!value.isNil()) {
            set(keywordArguments, name, value);
        }
    }
}

void logBlueprintCleanupFailure(const std::string& eventName,
                                const std::string& key,
                                const std::exception_ptr& failure) noexcept {
    try {
        std::rethrow_exception(failure);
    } catch (const std::exception& error) {
        std::cerr << "WARNING:Blueprint event '" << eventName
                  << "' failed to restore context key '" << key
                  << "': " << error.what() << '\n';
    } catch (...) {
        std::cerr << "WARNING:Blueprint event '" << eventName
                  << "' failed to restore context key '" << key
                  << "': unknown error\n";
    }
}

void logBlueprintCompletionFailure(const std::string& eventName,
                                   const std::exception_ptr& failure) noexcept {
    try {
        std::rethrow_exception(failure);
    } catch (const std::exception& error) {
        std::cerr << "WARNING:Blueprint event '" << eventName
                  << "' completion failed while preserving an earlier error: "
                  << error.what() << '\n';
    } catch (...) {
        std::cerr << "WARNING:Blueprint event '" << eventName
                  << "' completion failed while preserving an earlier error: "
                     "unknown error\n";
    }
}

bool executeBlueprintGraph(const RuntimeValue& graph,
                           const std::string& eventName,
                           const RuntimeValue& rawKeywordArguments,
                           const RuntimeValue& localGraph,
                           const std::function<void()>& onComplete) {
    const std::shared_ptr<Graph> nativeGraph = requireBlueprintGraph(graph);
    if (nativeGraph->getLatentPendingCount(eventName) > 0) {
        return false;
    }
    if (!nativeGraph->tryLockExecution(eventName)) {
        return false;
    }
    RuntimeIdentityPtr oldLocalGraph;
    RuntimeValue context;
    RuntimeValue oldContextGraph;
    std::vector<std::pair<std::string, RuntimeValue>> oldEventParameters;
    bool oldLocalGraphCaptured = false;
    bool contextGraphSet = false;
    std::exception_ptr failure;

    try {
        oldLocalGraph = nativeGraph->getLocalGraph();
        oldLocalGraphCaptured = true;
        context = RuntimeValue();
        oldContextGraph = RuntimeValue();
        if (onComplete) {
            nativeGraph->addExecutionCompleteCallback(eventName, onComplete);
        }
        if (!localGraph.isNil()) {
            nativeGraph->setLocalGraph(identity(localGraph));
        }
        RuntimeIdentityPtr activeLocalGraph = nativeGraph->getLocalGraph();
        if (activeLocalGraph == nullptr) {
            const NodeGraphContextObjects created = createNodeGraphContext(
                retain(makeValue(nativeGraph->parentClass)),
                retain(makeValue(nativeGraph->getParent())));
            activeLocalGraph = identity(created.localGraph);
            nativeGraph->setLocalGraph(activeLocalGraph);
        }
        if (activeLocalGraph == nullptr) {
            throw std::runtime_error("Blueprint graph has no local context");
        }

        context = retain(makeValue(activeLocalGraph));
        oldContextGraph = getNodeGraphContextValue(context, "__graph__");
        setNodeGraphContextValue(
            context, "__graph__",
            retain(makeValue(nativeGraph->getGraphContext())));
        contextGraphSet = true;
        if (isTable(rawKeywordArguments)) {
            for (const auto& entry : entries(rawKeywordArguments)) {
                if (!is<std::string>(entry.first)) {
                    continue;
                }
                const std::string name =
                    "__" + as<std::string>(entry.first) + "__";
                oldEventParameters.emplace_back(
                    name, getNodeGraphContextValue(context, name));
                setNodeGraphContextValue(context, name, entry.second);
            }
        }
        nativeGraph->execute(eventName);
    } catch (...) {
        failure = std::current_exception();
    }

    for (const auto& [name, value] : oldEventParameters) {
        try {
            setNodeGraphContextValue(context, name, value);
        } catch (...) {
            const std::exception_ptr restoreFailure = std::current_exception();
            logBlueprintCleanupFailure(eventName, name, restoreFailure);
            if (failure == nullptr) {
                failure = restoreFailure;
            }
        }
    }
    if (contextGraphSet) {
        try {
            setNodeGraphContextValue(context, "__graph__", oldContextGraph);
        } catch (...) {
            const std::exception_ptr restoreFailure = std::current_exception();
            logBlueprintCleanupFailure(eventName, "__graph__", restoreFailure);
            if (failure == nullptr) {
                failure = restoreFailure;
            }
        }
    }
    if (oldLocalGraphCaptured) {
        try {
            nativeGraph->setLocalGraph(oldLocalGraph);
        } catch (...) {
            const std::exception_ptr restoreFailure = std::current_exception();
            logBlueprintCleanupFailure(eventName, "localGraph", restoreFailure);
            if (failure == nullptr) {
                failure = restoreFailure;
            }
        }
    }

    try {
        nativeGraph->completeExecution(eventName);
    } catch (...) {
        const std::exception_ptr completionFailure = std::current_exception();
        if (failure != nullptr) {
            logBlueprintCompletionFailure(eventName, completionFailure);
        } else {
            failure = completionFailure;
        }
    }
    if (failure != nullptr) {
        std::rethrow_exception(failure);
    }
    return true;
}

}  // namespace ludork::engine::runtime_detail
