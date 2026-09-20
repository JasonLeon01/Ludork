#include <Runtime/RuntimeReference.hpp>
#include "BlueprintRuntimeInternal.hpp"

#include <Runtime/Components/ComponentRuntime.hpp>
#include <Blueprint/ClassRuntime/ClassRuntimeInternal.hpp>
#include <Runtime/NodeGraph/Graph.hpp>
#include <Runtime/Blueprint/ClassRuntime.hpp>
#include <NodeGraph/NodeGraphRuntime/NodeGraphRuntimeInternal.hpp>
#include <RuntimeSession.hpp>
#include <Utf8Path.hpp>
#include <Runtime/TypedDataService.hpp>

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

namespace ludork::runtime::blueprint_detail {

using namespace ludork::runtime::reference;
using namespace ludork::runtime::node_graph_detail;

std::shared_ptr<Graph> requireBlueprintGraph(const RuntimeValue& graph) {
    const RuntimeValue::Object* native = graph.getIf<RuntimeValue::Object>();
    const std::shared_ptr<Graph> nativeGraph =
        native != nullptr
            ? ludork::Cast<Graph>(*native)
            : (kind(graph) == "userdata"
                   ? ludork::Cast<Graph>(
                         ludork::runtime::reference::object(graph))
                   : nullptr);
    if (nativeGraph == nullptr) {
        throw std::invalid_argument("Blueprint graph must be an Engine.Graph");
    }
    return nativeGraph;
}

bool blueprintIsInstance(const RuntimeValue& value, const RuntimeValue& type) {
    return isTable(type) && isInstance(value, type);
}

RuntimeValue callRuntimeMethodFirst(
    const RuntimeValue& object, const char* name,
    const std::vector<RuntimeValue>& arguments) {
    const RuntimeValue method =
        get(ludork::runtime::reference::intern(object), name);
    if (!isFunction(method)) {
        return RuntimeValue();
    }
    std::vector<RuntimeValue> values;
    values.reserve(arguments.size() + 1);
    values.push_back(object);
    values.insert(values.end(), arguments.begin(), arguments.end());
    return first(invoke(ludork::runtime::reference::intern(method), values));
}

std::optional<double> runtimeNumber(const RuntimeValue& value) {
    const RuntimeValue rawToNumber = rawGet(globals(), "tonumber");
    if (!isFunction(rawToNumber)) {
        return std::nullopt;
    }
    RuntimeValue toNumber = rawToNumber;
    RuntimeValue::Array converted = invoke(
        ludork::runtime::reference::intern(toNumber), {makeValue(value)});
    const RuntimeValue result = first(converted);
    return is<double>(result) ? std::optional<double>(as<double>(result))
                              : std::nullopt;
}

bool blueprintGraphHasExecutableEvent(const std::shared_ptr<Graph>& graph,
                                      const std::string& eventName) {
    return graph != nullptr && graph->hasExecutableEvent(eventName);
}

bool blueprintGraphDataHasExecutableEvent(const RuntimeValue& graphData,
                                          const std::string& eventName) {
    if (!isTable(graphData)) {
        return false;
    }
    const RuntimeValue data = graphData;
    const RuntimeValue nodeGraph =
        get(ludork::runtime::reference::intern(data), "nodeGraph");
    const RuntimeValue startNodes =
        get(ludork::runtime::reference::intern(data), "startNodes");
    if (!isTable(nodeGraph) || !isTable(startNodes)) {
        return false;
    }
    const RuntimeValue eventGraph =
        get(ludork::runtime::reference::intern(nodeGraph), eventName);
    const RuntimeValue rawStart =
        get(ludork::runtime::reference::intern(startNodes), eventName);
    if (!isTable(eventGraph) || rawStart.isNil()) {
        return false;
    }
    const RuntimeValue nodes =
        get(ludork::runtime::reference::intern(eventGraph), "nodes");
    const std::optional<double> start = runtimeNumber(rawStart);
    return isTable(nodes) && start.has_value() && *start >= 0.0 &&
           *start < static_cast<double>(
                        length(ludork::runtime::reference::intern(nodes)));
}

bool generatedBlueprintGraphHasExecutableEvent(const RuntimeValue& classType,
                                               const std::string& eventName) {
    const RuntimeValue rawScriptMixin =
        get(ludork::runtime::reference::intern(classType), "scriptMixin");
    if (is<bool>(rawScriptMixin) && as<bool>(rawScriptMixin)) {
        return false;
    }
    const RuntimeValue rawPath = rawGet(
        ludork::runtime::reference::intern(classType), "__blueprintClassPath");
    if (!is<std::string>(rawPath)) {
        return false;
    }
    return ludork::runtime::class_runtime_detail::classGraphHasExecutableEvent(
        as<std::string>(rawPath), eventName);
}

std::shared_ptr<Graph> generatedBlueprintGraph(const RuntimeValue& object,
                                               const RuntimeValue& classType) {
    RuntimeValue rawCache =
        get(ludork::runtime::reference::intern(object), "_parentGraphs");
    RuntimeValue cache = isTable(rawCache) ? rawCache : table();
    if (!isTable(rawCache)) {
        set(ludork::runtime::reference::intern(object), "_parentGraphs", cache);
    }
    RuntimeValue graph =
        rawGet(ludork::runtime::reference::intern(cache), classType);
    if (graph.isNil()) {
        const RuntimeValue rawPath =
            rawGet(ludork::runtime::reference::intern(classType),
                   "__blueprintClassPath");
        if (!is<std::string>(rawPath)) {
            return nullptr;
        }
        graph = RuntimeValue(
            ludork::runtime::class_runtime_detail::instantiateClassGraph(
                as<std::string>(rawPath), object));
        if (!graph.isNil()) {
            rawSet(ludork::runtime::reference::intern(cache), classType, graph);
        }
    }
    return graph.isNil() ? nullptr : requireBlueprintGraph(graph);
}

EventArguments blueprintEventArguments(const RuntimeValue& classType,
                                       const std::string& eventName,
                                       const RuntimeValue& rawArguments,
                                       const EventArguments& arguments) {
    EventArguments result = arguments;
    if (!isTable(rawArguments)) {
        return result;
    }
    const RuntimeHandle positional = intern(rawArguments);
    if (length(positional) == 0) {
        return result;
    }
    const RuntimeValue method = get(intern(classType), eventName);
    if (!isFunction(method)) {
        return result;
    }
    const auto descriptor =
        runtimeEventDescriptor(method, classType, eventName);
    const std::size_t count =
        std::min(length(positional), descriptor->parameters.size());
    for (std::size_t index = 0; index < count; ++index) {
        const std::string& name = descriptor->parameters[index];
        const auto found = std::find_if(
            result.begin(), result.end(),
            [&name](const BlueprintRuntimeFacade::EventArgument& argument) {
                return argument.name == name;
            });
        if (found == result.end() || found->value.isNil()) {
            const RuntimeValue value = get(positional, index + 1);
            if (found != result.end()) {
                result.erase(found);
            }
            if (!value.isNil()) {
                result.push_back({name, value});
            }
        }
    }
    return result;
}

void mergeBlueprintLocalArguments(const RuntimeValue& classType,
                                  const std::string& eventName,
                                  EventArguments& arguments,
                                  const RuntimeValue& localGraph) {
    if (!isTable(localGraph)) {
        return;
    }
    const RuntimeValue method = get(intern(classType), eventName);
    if (!isFunction(method)) {
        return;
    }
    const auto descriptor =
        runtimeEventDescriptor(method, classType, eventName);
    for (const std::string& name : descriptor->parameters) {
        const auto found = std::find_if(
            arguments.begin(), arguments.end(),
            [&name](const BlueprintRuntimeFacade::EventArgument& argument) {
                return argument.name == name;
            });
        if (found != arguments.end() && !found->value.isNil()) {
            continue;
        }
        const RuntimeValue value = get(intern(localGraph), "__" + name + "__");
        if (!value.isNil()) {
            if (found == arguments.end()) {
                arguments.push_back({name, value});
            } else {
                found->value = value;
            }
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

bool executeBlueprintGraph(const std::shared_ptr<Graph>& nativeGraph,
                           const std::string& eventName,
                           const EventArguments& arguments,
                           const RuntimeValue& localGraph,
                           const std::function<void()>& onComplete) {
    if (nativeGraph == nullptr) {
        throw std::invalid_argument("Blueprint graph must be an Engine.Graph");
    }
    if (nativeGraph->getLatentPendingCount(eventName) > 0) {
        return false;
    }
    if (!nativeGraph->tryLockExecution(eventName)) {
        return false;
    }
    RuntimeIdentityPtr oldLocalGraph;
    RuntimeScope scope;
    RuntimeHandle context;
    RuntimeValue oldContextGraph;
    std::vector<std::pair<std::string, RuntimeValue>> oldEventParameters;
    bool oldLocalGraphCaptured = false;
    bool contextGraphSet = false;
    std::exception_ptr failure;

    try {
        oldLocalGraph = nativeGraph->getLocalGraph();
        oldLocalGraphCaptured = true;
        context = RuntimeHandle();
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
                scope, nativeGraph->parentClass, nativeGraph->getParent());
            activeLocalGraph = created.localGraph.identity();
            nativeGraph->setLocalGraph(activeLocalGraph);
        }
        if (activeLocalGraph == nullptr) {
            throw std::runtime_error("Blueprint graph has no local context");
        }

        context = RuntimeHandle(activeLocalGraph);
        oldContextGraph = getNodeGraphContextValue(scope, context, "__graph__");
        setNodeGraphContextValue(scope, context, "__graph__",
                                 nativeGraph->getGraphContext());
        contextGraphSet = true;
        for (const BlueprintRuntimeFacade::EventArgument& argument :
             arguments) {
            const std::string name = "__" + argument.name + "__";
            oldEventParameters.emplace_back(
                name, getNodeGraphContextValue(scope, context, name));
            setNodeGraphContextValue(scope, context, name, argument.value);
        }
        nativeGraph->execute(eventName);
    } catch (...) {
        failure = std::current_exception();
    }

    for (const auto& [name, value] : oldEventParameters) {
        try {
            setNodeGraphContextValue(scope, context, name, value);
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
            setNodeGraphContextValue(scope, context, "__graph__",
                                     oldContextGraph);
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

}  // namespace ludork::runtime::blueprint_detail
