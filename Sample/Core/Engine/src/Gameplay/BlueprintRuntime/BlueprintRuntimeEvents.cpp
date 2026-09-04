#include "BlueprintRuntimeInternal.hpp"
#include <EngineRuntimeServices.hpp>

#include <ClassServices.hpp>
#include <Gameplay/Components/ComponentRuntime.hpp>
#include <LudorkRuntimeBinding/FunctionAdapter.hpp>
#include <NodeGraph/Graph.hpp>
#include <Gameplay/EngineClassRuntime.hpp>
#include <RuntimeSession.hpp>
#include <Utf8Path.hpp>
#include <Utils/DataValue.hpp>

extern "C" {
#include <lua.h>
}

#include <algorithm>
#include <climits>
#include <cstddef>
#include <fstream>
#include <functional>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::engine::runtime_detail {

std::function<void()> completionCallback(const sol::object& value) {
    if (!value.valid() || value.get_type() == sol::type::none ||
        value.get_type() == sol::type::lua_nil) {
        return {};
    }
    return ludork::runtime::binding::readLuaValue<std::function<void()>>(value);
}

void invokeCompletion(const std::function<void()>& callback) {
    if (callback) {
        callback();
    }
}

bool hasBlueprintEvent(sol::this_state state, const sol::object& object,
                       const std::string& eventName);

void dispatchBlueprintEvent(sol::this_state state, const sol::object& object,
                            const sol::object& rawObjectType,
                            const std::string& eventName,
                            const sol::object& rawKeywordArguments,
                            const std::function<void()>& onComplete);

void validateBlueprintEvent(sol::this_state state, const sol::object& object,
                            const std::string& eventName) {
    sol::state_view lua(state);
    if (!object.valid() || object.get_type() == sol::type::lua_nil) {
        throw std::invalid_argument("Blueprint event target object is nil");
    }
    if (!hasBlueprintEvent(state, object, eventName)) {
        throw std::invalid_argument("Object has no blueprint event '" +
                                    eventName + "'");
    }
}

void invokeBlueprintEvent(sol::this_state state, const sol::object& object,
                          const std::string& eventName) {
    sol::state_view lua(state);
    dispatchBlueprintEvent(state, object, classType(state, object), eventName,
                           sol::make_object(lua, lua.create_table()), {});
}

bool classHasBlueprintEvent(sol::this_state state, const sol::object& rawClass,
                            const std::string& eventName) {
    sol::state_view lua(state);
    if (!rawClass.is<sol::table>()) {
        return false;
    }
    const sol::table classType = rawClass.as<sol::table>();
    if (!isClass(classType) && !isNativeType(lua, classType)) {
        return false;
    }
    const sol::object generated =
        classType.raw_get<sol::object>("_GENERATED_CLASS");
    if (luaBoolean(generated)) {
        const sol::object rawScriptMixin =
            classType.get<sol::object>("scriptMixin");
        if (rawScriptMixin.is<bool>() && rawScriptMixin.as<bool>()) {
            const sol::object method =
                classType.raw_get<sol::object>(eventName);
            if (runtimeMethodHasImplementation(lua, method)) {
                return true;
            }
            return classHasBlueprintEvent(
                state, classType.raw_get<sol::object>("__base"), eventName);
        }
        if (generatedBlueprintGraphHasExecutableEvent(lua, classType,
                                                      eventName)) {
            return true;
        }
        return classHasBlueprintEvent(
            state, classType.raw_get<sol::object>("__base"), eventName);
    }
    const sol::object graph = classType.raw_get<sol::object>("_graph");
    if (blueprintGraphHasExecutableEvent(lua, graph, eventName)) {
        return true;
    }
    const sol::object method = classType.raw_get<sol::object>(eventName);
    if (isClass(classType) && runtimeMethodHasImplementation(lua, method)) {
        return true;
    }
    return classHasBlueprintEvent(
        state, classType.raw_get<sol::object>("__base"), eventName);
}

bool hasBlueprintEvent(sol::this_state state, const sol::object& object,
                       const std::string& eventName) {
    sol::state_view lua(state);
    if (!object.valid() || object.get_type() == sol::type::lua_nil ||
        eventName.empty()) {
        return false;
    }
    const sol::object rawClass = classType(state, object);
    const bool scriptMixin =
        rawClass.is<sol::table>() &&
        rawClass.as<sol::table>().get<sol::object>("scriptMixin").is<bool>() &&
        rawClass.as<sol::table>().get<sol::object>("scriptMixin").as<bool>();
    const sol::object actorType = blueprintEngineType(lua, "Actor");
    const sol::object actorGraph =
        !scriptMixin && blueprintIsInstance(state, object, actorType)
            ? callRuntimeMethodFirst(lua, object, "getGraph")
            : nilObject(lua);
    if (blueprintGraphHasExecutableEvent(lua, actorGraph, eventName)) {
        return true;
    }
    sol::object instanceMethod = nilObject(lua);
    instanceMethod = ludork::standard::class_runtime::rawGetOwnField(
        lua, object, sol::make_object(lua, eventName));
    if (runtimeMethodHasImplementation(lua, instanceMethod)) {
        return true;
    }
    return classHasBlueprintEvent(state, rawClass, eventName);
}

bool executeParentBlueprintEvent(
    sol::this_state state, const sol::object& object,
    const sol::object& rawClass, const std::string& eventName,
    const sol::object& arguments, const sol::object& keywordArguments,
    const sol::object& localGraph, const std::function<void()>& onComplete) {
    sol::state_view lua(state);
    if (!rawClass.is<sol::table>()) {
        return false;
    }
    const sol::object rawParent =
        rawClass.as<sol::table>().raw_get<sol::object>("__base");
    if (!rawParent.is<sol::table>()) {
        return false;
    }
    const sol::table parent = rawParent.as<sol::table>();
    sol::table eventArguments = blueprintEventKeywordArguments(
        lua, parent, eventName, arguments, keywordArguments);
    mergeBlueprintLocalArguments(lua, parent, eventName, eventArguments,
                                 localGraph);
    if (luaBoolean(parent.raw_get<sol::object>("_GENERATED_CLASS"))) {
        if (generatedBlueprintGraphHasExecutableEvent(lua, parent, eventName)) {
            const sol::object graph =
                generatedBlueprintGraph(lua, object, parent);
            if (graph.valid() && graph.get_type() != sol::type::lua_nil) {
                if (!executeBlueprintGraph(
                        lua, graph, eventName,
                        sol::make_object(lua, eventArguments), localGraph,
                        onComplete)) {
                    invokeCompletion(onComplete);
                }
                return true;
            }
        }
        return executeParentBlueprintEvent(
            state, object, rawParent, eventName, nilObject(lua),
            sol::make_object(lua, eventArguments), localGraph, onComplete);
    }

    const sol::object graph = parent.get<sol::object>("_graph");
    if (graph.valid() && graph.get_type() != sol::type::lua_nil &&
        luaBoolean(callRuntimeMethodFirst(
            lua, graph, "hasKey", {sol::make_object(lua, eventName)}))) {
        const sol::object startNodes = runtimeIndex(
            lua, graph, sol::make_object(lua, "startNodes"), false);
        if (startNodes.is<sol::table>() &&
            startNodes.as<sol::table>()
                    .get<sol::object>(eventName)
                    .get_type() != sol::type::lua_nil) {
            if (!executeBlueprintGraph(lua, graph, eventName,
                                       sol::make_object(lua, eventArguments),
                                       localGraph, onComplete)) {
                invokeCompletion(onComplete);
            }
            return true;
        }
        return executeParentBlueprintEvent(
            state, object, rawParent, eventName, nilObject(lua),
            sol::make_object(lua, eventArguments), localGraph, onComplete);
    }

    const sol::object method = parent.get<sol::object>(eventName);
    if (!method.is<sol::protected_function>()) {
        return executeParentBlueprintEvent(
            state, object, rawParent, eventName, nilObject(lua),
            sol::make_object(lua, eventArguments), localGraph, onComplete);
    }
    invokeNamedRuntimeMethod(lua, object, method, parent, eventName,
                             sol::make_object(lua, eventArguments));
    invokeCompletion(onComplete);
    return true;
}

void dispatchBlueprintEvent(sol::this_state state, const sol::object& object,
                            const sol::object& rawObjectType,
                            const std::string& eventName,
                            const sol::object& rawKeywordArguments,
                            const std::function<void()>& onComplete) {
    sol::state_view lua(state);
    const sol::object isDestroyed =
        runtimeIndex(lua, object, sol::make_object(lua, "isDestroyed"), false);
    if (isDestroyed.is<sol::protected_function>() &&
        luaBoolean(callRuntimeMethodFirst(lua, object, "isDestroyed"))) {
        invokeCompletion(onComplete);
        return;
    }
    const sol::object objectType = rawObjectType.is<sol::table>()
                                       ? rawObjectType
                                       : classType(state, object);
    if (!blueprintIsInstance(state, object, objectType)) {
        invokeCompletion(onComplete);
        return;
    }
    const sol::table keywordArguments =
        rawKeywordArguments.is<sol::table>()
            ? rawKeywordArguments.as<sol::table>()
            : lua.create_table();
    const sol::object rawClass = classType(state, object);
    const bool scriptMixin =
        rawClass.is<sol::table>() &&
        rawClass.as<sol::table>().get<sol::object>("scriptMixin").is<bool>() &&
        rawClass.as<sol::table>().get<sol::object>("scriptMixin").as<bool>();
    if (scriptMixin) {
        const sol::object method =
            runtimeIndex(lua, object, sol::make_object(lua, eventName), false);
        invokeNamedRuntimeMethod(lua, object, method, rawClass.as<sol::table>(),
                                 eventName,
                                 sol::make_object(lua, keywordArguments));
        invokeCompletion(onComplete);
        return;
    }
    const sol::object actorType = blueprintEngineType(lua, "Actor");
    const sol::object graph =
        blueprintIsInstance(state, object, actorType)
            ? callRuntimeMethodFirst(lua, object, "getGraph")
            : nilObject(lua);
    const bool generated =
        rawClass.is<sol::table>() &&
        luaBoolean(
            rawClass.as<sol::table>().raw_get<sol::object>("_GENERATED_CLASS"));
    if (generated && graph.valid() && graph.get_type() != sol::type::lua_nil) {
        if (luaBoolean(callRuntimeMethodFirst(
                lua, graph, "hasKey", {sol::make_object(lua, eventName)}))) {
            const sol::object startNodes = runtimeIndex(
                lua, graph, sol::make_object(lua, "startNodes"), false);
            if (startNodes.is<sol::table>() &&
                startNodes.as<sol::table>()
                        .get<sol::object>(eventName)
                        .get_type() != sol::type::lua_nil) {
                if (!executeBlueprintGraph(
                        lua, graph, eventName,
                        sol::make_object(lua, keywordArguments), nilObject(lua),
                        onComplete)) {
                    invokeCompletion(onComplete);
                }
                return;
            }
        }
        if (executeParentBlueprintEvent(state, object, rawClass, eventName,
                                        nilObject(lua),
                                        sol::make_object(lua, keywordArguments),
                                        nilObject(lua), onComplete)) {
            return;
        }
        const sol::object method =
            runtimeIndex(lua, object, sol::make_object(lua, eventName), false);
        invokeNamedRuntimeMethod(lua, object, method, rawClass.as<sol::table>(),
                                 eventName,
                                 sol::make_object(lua, keywordArguments));
        invokeCompletion(onComplete);
        return;
    }
    const sol::object method =
        runtimeIndex(lua, object, sol::make_object(lua, eventName), false);
    invokeNamedRuntimeMethod(lua, object, method, rawClass.as<sol::table>(),
                             eventName,
                             sol::make_object(lua, keywordArguments));
    invokeCompletion(onComplete);
}

}  // namespace ludork::engine::runtime_detail
