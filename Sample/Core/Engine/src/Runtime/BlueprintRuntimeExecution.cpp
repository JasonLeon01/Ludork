#include "BlueprintRuntimeInternal.hpp"
#include <Runtime/EngineRuntimeServices.hpp>

#include <ClassServices.hpp>
#include <Gameplay/Components/ComponentRuntime.hpp>
#include <LudorkCoreBinding/DynamicValueCodec.hpp>
#include <NodeGraph/Graph.hpp>
#include <Runtime/EngineClassRuntime.hpp>
#include <Runtime/NodeGraphRuntime.hpp>
#include <RuntimeSession.hpp>
#include <Utf8Path.hpp>
#include <Utils/DataValue.hpp>

extern "C" {
#include <lua.h>
}

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

sol::object blueprintEngineType(sol::state_view lua, const char* name) {
    return requireRuntimeType(lua, "Engine", name);
}

bool blueprintIsInstance(sol::this_state state, const sol::object& value,
                         const sol::object& type) {
    return type.is<sol::table>() &&
           isInstance(state, value, type.as<sol::table>());
}

sol::object callRuntimeMethodFirst(sol::state_view lua,
                                   const sol::object& object, const char* name,
                                   const std::vector<sol::object>& arguments) {
    const sol::object method =
        runtimeIndex(lua, object, sol::make_object(lua, name), false);
    if (!method.is<sol::protected_function>()) {
        return nilObject(lua);
    }
    std::vector<sol::object> values;
    values.reserve(arguments.size() + 1);
    values.push_back(object);
    values.insert(values.end(), arguments.begin(), arguments.end());
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    try {
        const int resultCount = invokeRuntimeFunction(
            state, method, values, "runtime method arguments");
        sol::object result = resultCount == 0 ? nilObject(lua)
                                              : sol::stack::get<sol::object>(
                                                    state, stackBase + 1);
        lua_settop(state, stackBase);
        return result;
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

std::optional<double> runtimeNumber(sol::state_view lua,
                                    const sol::object& value) {
    const sol::object rawToNumber =
        lua.globals().raw_get<sol::object>("tonumber");
    if (!rawToNumber.is<sol::protected_function>()) {
        return std::nullopt;
    }
    sol::protected_function toNumber =
        rawToNumber.as<sol::protected_function>();
    sol::protected_function_result converted = toNumber(value);
    const sol::object result = checkedResult(lua, converted);
    return result.is<double>() ? std::optional<double>(result.as<double>())
                               : std::nullopt;
}

bool blueprintGraphHasExecutableEvent(sol::state_view lua,
                                      const sol::object& graph,
                                      const std::string& eventName) {
    if (!graph.valid() || graph.get_type() == sol::type::lua_nil) {
        return false;
    }
    const sol::object direct = runtimeIndex(
        lua, graph, sol::make_object(lua, "hasExecutableEvent"), false);
    if (direct.is<sol::protected_function>()) {
        return luaBoolean(
            callRuntimeMethodFirst(lua, graph, "hasExecutableEvent",
                                   {sol::make_object(lua, eventName)}));
    }
    const sol::object startNodes =
        runtimeIndex(lua, graph, sol::make_object(lua, "startNodes"), false);
    const sol::object nodes =
        runtimeIndex(lua, graph, sol::make_object(lua, "nodes"), false);
    if (!startNodes.is<sol::table>() || !nodes.is<sol::table>()) {
        return false;
    }
    const sol::object rawStart =
        startNodes.as<sol::table>().get<sol::object>(eventName);
    const sol::object rawEventNodes =
        nodes.as<sol::table>().get<sol::object>(eventName);
    if (rawStart.get_type() == sol::type::lua_nil ||
        !rawEventNodes.is<sol::table>()) {
        return false;
    }
    const std::optional<double> start = runtimeNumber(lua, rawStart);
    return start.has_value() && *start >= 0.0 &&
           *start < static_cast<double>(rawEventNodes.as<sol::table>().size());
}

bool blueprintGraphDataHasExecutableEvent(sol::state_view lua,
                                          const sol::object& graphData,
                                          const std::string& eventName) {
    if (!graphData.is<sol::table>()) {
        return false;
    }
    const sol::table data = graphData.as<sol::table>();
    const sol::object nodeGraph = data.get<sol::object>("nodeGraph");
    const sol::object startNodes = data.get<sol::object>("startNodes");
    if (!nodeGraph.is<sol::table>() || !startNodes.is<sol::table>()) {
        return false;
    }
    const sol::object eventGraph =
        nodeGraph.as<sol::table>().get<sol::object>(eventName);
    const sol::object rawStart =
        startNodes.as<sol::table>().get<sol::object>(eventName);
    if (!eventGraph.is<sol::table>() ||
        rawStart.get_type() == sol::type::lua_nil) {
        return false;
    }
    const sol::object nodes =
        eventGraph.as<sol::table>().get<sol::object>("nodes");
    const std::optional<double> start = runtimeNumber(lua, rawStart);
    return nodes.is<sol::table>() && start.has_value() && *start >= 0.0 &&
           *start < static_cast<double>(nodes.as<sol::table>().size());
}

bool generatedBlueprintGraphHasExecutableEvent(sol::state_view lua,
                                               const sol::table& classType,
                                               const std::string& eventName) {
    const sol::object rawScriptMixin =
        classType.get<sol::object>("scriptMixin");
    if (rawScriptMixin.is<bool>() && rawScriptMixin.as<bool>()) {
        return false;
    }
    const sol::object rawPath =
        classType.raw_get<sol::object>("__blueprintClassPath");
    if (!rawPath.is<std::string>()) {
        return false;
    }
    const sol::object result = callRegisteredRuntimeServiceFirst(
        lua, "nodegraph.classGraphHasExecutableEvent",
        {rawPath, sol::make_object(lua, eventName)});
    return luaBoolean(result);
}

sol::object generatedBlueprintGraph(sol::state_view lua,
                                    const sol::object& object,
                                    const sol::table& classType) {
    sol::object rawCache = runtimeIndex(
        lua, object, sol::make_object(lua, "_parentGraphs"), false);
    sol::table cache = rawCache.is<sol::table>() ? rawCache.as<sol::table>()
                                                 : lua.create_table();
    if (!rawCache.is<sol::table>()) {
        runtimeAssign(lua, object, sol::make_object(lua, "_parentGraphs"),
                      sol::make_object(lua, cache), false);
    }
    sol::object graph = cache.raw_get<sol::object>(classType);
    if (graph.get_type() == sol::type::lua_nil) {
        const sol::object rawPath =
            classType.raw_get<sol::object>("__blueprintClassPath");
        if (!rawPath.is<std::string>()) {
            return nilObject(lua);
        }
        const sol::object result = callRegisteredRuntimeServiceFirst(
            lua, "nodegraph.instantiateClassGraph", {rawPath, object});
        graph = result;
        if (graph.valid() && graph.get_type() != sol::type::lua_nil) {
            cache.raw_set(classType, graph);
        }
    }
    return graph;
}

sol::table blueprintEventKeywordArguments(
    sol::state_view lua, const sol::table& classType,
    const std::string& eventName, const sol::object& rawArguments,
    const sol::object& rawKeywordArguments) {
    sol::table result = lua.create_table();
    if (rawKeywordArguments.is<sol::table>()) {
        for (const auto& entry : rawKeywordArguments.as<sol::table>()) {
            result.raw_set(entry.first, entry.second);
        }
    }
    if (!rawArguments.is<sol::table>() ||
        rawArguments.as<sol::table>().size() == 0) {
        return result;
    }
    const sol::object method = classType.get<sol::object>(eventName);
    if (!method.is<sol::protected_function>()) {
        return result;
    }
    const sol::table names = runtimeDescriptorParameters(
        lua, runtimeEventDescriptor(lua, method, classType, eventName));
    const sol::table arguments = rawArguments.as<sol::table>();
    const std::size_t count = std::min(arguments.size(), names.size());
    for (std::size_t index = 1; index <= count; ++index) {
        const sol::object rawName = names.raw_get<sol::object>(index);
        if (!rawName.is<std::string>()) {
            continue;
        }
        const std::string name = rawName.as<std::string>();
        if (result.get<sol::object>(name).get_type() == sol::type::lua_nil) {
            result.set(name, arguments.get<sol::object>(index));
        }
    }
    return result;
}

void mergeBlueprintLocalArguments(sol::state_view lua,
                                  const sol::table& classType,
                                  const std::string& eventName,
                                  sol::table keywordArguments,
                                  const sol::object& localGraph) {
    if (!localGraph.is<sol::table>()) {
        return;
    }
    const sol::object method = classType.get<sol::object>(eventName);
    if (!method.is<sol::protected_function>()) {
        return;
    }
    const sol::table names = runtimeDescriptorParameters(
        lua, runtimeEventDescriptor(lua, method, classType, eventName));
    for (std::size_t index = 1; index <= names.size(); ++index) {
        const sol::object rawName = names.raw_get<sol::object>(index);
        if (!rawName.is<std::string>()) {
            continue;
        }
        const std::string name = rawName.as<std::string>();
        if (keywordArguments.get<sol::object>(name).get_type() !=
            sol::type::lua_nil) {
            continue;
        }
        const sol::object value =
            localGraph.as<sol::table>().get<sol::object>("__" + name + "__");
        if (value.get_type() != sol::type::lua_nil) {
            keywordArguments.set(name, value);
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

bool executeBlueprintGraph(sol::state_view lua, const sol::object& graph,
                           const std::string& eventName,
                           const sol::object& rawKeywordArguments,
                           const sol::object& localGraph,
                           const std::function<void()>& onComplete) {
    const std::shared_ptr<Graph> nativeGraph =
        graph.as<std::shared_ptr<Graph>>();
    if (!nativeGraph->tryLockExecution(eventName)) {
        return false;
    }
    RuntimeIdentityPtr oldLocalGraph;
    sol::object context;
    sol::object oldContextGraph;
    std::vector<std::pair<std::string, sol::object>> oldEventParameters;
    bool oldLocalGraphCaptured = false;
    bool contextGraphSet = false;
    std::exception_ptr failure;

    try {
        oldLocalGraph = nativeGraph->getLocalGraph();
        oldLocalGraphCaptured = true;
        context = nilObject(lua);
        oldContextGraph = nilObject(lua);
        if (onComplete) {
            nativeGraph->addExecutionCompleteCallback(eventName, onComplete);
        }
        if (localGraph.valid() && localGraph.get_type() != sol::type::lua_nil) {
            nativeGraph->setLocalGraph(
                ludork_core::readLuaValue<RuntimeIdentityPtr>(localGraph));
        }
        RuntimeIdentityPtr activeLocalGraph = nativeGraph->getLocalGraph();
        if (activeLocalGraph == nullptr) {
            const NodeGraphContextObjects created = createNodeGraphContext(
                lua, ludork_core::writeLuaValue(lua, nativeGraph->parentClass),
                ludork_core::writeLuaValue(lua, nativeGraph->getParent()));
            activeLocalGraph = ludork_core::readLuaValue<RuntimeIdentityPtr>(
                created.localGraph);
            nativeGraph->setLocalGraph(activeLocalGraph);
        }
        if (activeLocalGraph == nullptr) {
            throw std::runtime_error("Blueprint graph has no local context");
        }

        context = ludork_core::writeLuaValue(lua, activeLocalGraph);
        oldContextGraph = getNodeGraphContextValue(lua, context, "__graph__");
        setNodeGraphContextValue(
            lua, context, "__graph__",
            ludork_core::writeLuaValue(lua, nativeGraph->getGraphContext()));
        contextGraphSet = true;
        if (rawKeywordArguments.is<sol::table>()) {
            for (const auto& entry : rawKeywordArguments.as<sol::table>()) {
                if (!entry.first.is<std::string>()) {
                    continue;
                }
                const std::string name =
                    "__" + entry.first.as<std::string>() + "__";
                oldEventParameters.emplace_back(
                    name, getNodeGraphContextValue(lua, context, name));
                setNodeGraphContextValue(lua, context, name, entry.second);
            }
        }
        nativeGraph->execute(eventName);
    } catch (...) {
        failure = std::current_exception();
    }

    for (const auto& [name, value] : oldEventParameters) {
        try {
            setNodeGraphContextValue(lua, context, name, value);
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
            setNodeGraphContextValue(lua, context, "__graph__",
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

bool tryExecuteInfoBlueprintGraph(sol::this_state state,
                                  const sol::object& object,
                                  const std::string& eventName,
                                  const sol::object& keywordArguments,
                                  const std::function<void()>& onComplete) {
    sol::state_view lua(state);
    const sol::object infoBase = blueprintEngineType(lua, "InfoBase");
    if (!blueprintIsInstance(state, object, infoBase)) {
        return false;
    }
    const sol::object graph =
        callRuntimeMethodFirst(lua, object, "getInfoGraph");
    if (!blueprintGraphHasExecutableEvent(lua, graph, eventName)) {
        return false;
    }
    if (!executeBlueprintGraph(lua, graph, eventName, keywordArguments,
                               nilObject(lua), onComplete)) {
        invokeCompletion(onComplete);
    }
    return true;
}

}  // namespace ludork::engine::runtime_detail
