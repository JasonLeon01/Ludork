#include <Runtime/RuntimeReference.hpp>
#include "BlueprintRuntimeInternal.hpp"
#include <EngineRuntimeServices.hpp>

#include <Gameplay/Components/ComponentRuntime.hpp>
#include <NodeGraph/Graph.hpp>
#include <Gameplay/EngineClassRuntime.hpp>
#include <RuntimeSession.hpp>
#include <Utf8Path.hpp>
#include <Utils/DataValue.hpp>

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

using namespace ludork::runtime::reference;

std::function<void()> completionCallback(const RuntimeValue& value) {
    if (value.isNil()) {
        return {};
    }
    if (!isFunction(value)) {
        throw std::invalid_argument("Blueprint completion must be a function");
    }
    return [value] {
        static_cast<void>(invoke(value));
    };
}

void invokeCompletion(const std::function<void()>& callback) {
    if (callback) {
        callback();
    }
}

bool hasBlueprintEvent(const RuntimeValue& object,
                       const std::string& eventName);

void dispatchBlueprintEvent(const RuntimeValue& object,
                            const RuntimeValue& rawObjectType,
                            const std::string& eventName,
                            const RuntimeValue& rawKeywordArguments,
                            const std::function<void()>& onComplete);

void validateBlueprintEvent(const RuntimeValue& object,
                            const std::string& eventName) {
    if (object.isNil()) {
        throw std::invalid_argument("Blueprint event target object is nil");
    }
    if (!hasBlueprintEvent(object, eventName)) {
        throw std::invalid_argument("Object has no blueprint event '" +
                                    eventName + "'");
    }
}

void invokeBlueprintEvent(const RuntimeValue& object,
                          const std::string& eventName) {
    dispatchBlueprintEvent(object, classType(object), eventName,
                           retain(makeValue(table())), {});
}

bool classHasBlueprintEvent(const RuntimeValue& rawClass,
                            const std::string& eventName) {
    if (!isTable(rawClass)) {
        return false;
    }
    const RuntimeValue classType = rawClass;
    if (!isClass(classType) && !isNativeType(classType)) {
        return false;
    }
    const RuntimeValue generated = rawGet(classType, "_GENERATED_CLASS");
    if (boolean(generated)) {
        const RuntimeValue rawScriptMixin = get(classType, "scriptMixin");
        if (is<bool>(rawScriptMixin) && as<bool>(rawScriptMixin)) {
            const RuntimeValue method = rawGet(classType, eventName);
            if (runtimeMethodHasImplementation(method)) {
                return true;
            }
            return classHasBlueprintEvent(rawGet(classType, "__base"),
                                          eventName);
        }
        if (generatedBlueprintGraphHasExecutableEvent(classType, eventName)) {
            return true;
        }
        return classHasBlueprintEvent(rawGet(classType, "__base"), eventName);
    }
    const RuntimeValue graph = rawGet(classType, "_graph");
    if (blueprintGraphHasExecutableEvent(graph, eventName)) {
        return true;
    }
    const RuntimeValue method = rawGet(classType, eventName);
    if (isClass(classType) && runtimeMethodHasImplementation(method)) {
        return true;
    }
    return classHasBlueprintEvent(rawGet(classType, "__base"), eventName);
}

bool hasBlueprintEvent(const RuntimeValue& object,
                       const std::string& eventName) {
    if (object.isNil() || eventName.empty()) {
        return false;
    }
    const RuntimeValue rawClass = classType(object);
    const bool scriptMixin = isTable(rawClass) &&
                             is<bool>(get(rawClass, "scriptMixin")) &&
                             as<bool>(get(rawClass, "scriptMixin"));
    const RuntimeValue actorType = blueprintEngineType("Actor");
    const RuntimeValue actorGraph =
        !scriptMixin && blueprintIsInstance(object, actorType)
            ? callRuntimeMethodFirst(object, "getGraph")
            : RuntimeValue();
    if (blueprintGraphHasExecutableEvent(actorGraph, eventName)) {
        return true;
    }
    RuntimeValue instanceMethod = RuntimeValue();
    instanceMethod = rawGet(object, retain(makeValue(eventName)));
    if (runtimeMethodHasImplementation(instanceMethod)) {
        return true;
    }
    return classHasBlueprintEvent(rawClass, eventName);
}

bool executeParentBlueprintEvent(const RuntimeValue& object,
                                 const RuntimeValue& rawClass,
                                 const std::string& eventName,
                                 const RuntimeValue& arguments,
                                 const RuntimeValue& keywordArguments,
                                 const RuntimeValue& localGraph,
                                 const std::function<void()>& onComplete) {
    if (!isTable(rawClass)) {
        return false;
    }
    const RuntimeValue rawParent = rawGet(rawClass, "__base");
    if (!isTable(rawParent)) {
        return false;
    }
    const RuntimeValue parent = rawParent;
    RuntimeValue eventArguments = blueprintEventKeywordArguments(
        parent, eventName, arguments, keywordArguments);
    mergeBlueprintLocalArguments(parent, eventName, eventArguments, localGraph);
    if (boolean(rawGet(parent, "_GENERATED_CLASS"))) {
        if (generatedBlueprintGraphHasExecutableEvent(parent, eventName)) {
            const RuntimeValue graph = generatedBlueprintGraph(object, parent);
            if (!graph.isNil()) {
                if (!executeBlueprintGraph(graph, eventName,
                                           retain(makeValue(eventArguments)),
                                           localGraph, onComplete)) {
                    invokeCompletion(onComplete);
                }
                return true;
            }
        }
        return executeParentBlueprintEvent(
            object, rawParent, eventName, RuntimeValue(),
            retain(makeValue(eventArguments)), localGraph, onComplete);
    }

    const RuntimeValue graph = get(parent, "_graph");
    if (!graph.isNil() && requireBlueprintGraph(graph)->hasKey(eventName)) {
        if (requireBlueprintGraph(graph)->startNodes.contains(eventName)) {
            if (!executeBlueprintGraph(graph, eventName,
                                       retain(makeValue(eventArguments)),
                                       localGraph, onComplete)) {
                invokeCompletion(onComplete);
            }
            return true;
        }
        return executeParentBlueprintEvent(
            object, rawParent, eventName, RuntimeValue(),
            retain(makeValue(eventArguments)), localGraph, onComplete);
    }

    const RuntimeValue method = get(parent, eventName);
    if (!isFunction(method)) {
        return executeParentBlueprintEvent(
            object, rawParent, eventName, RuntimeValue(),
            retain(makeValue(eventArguments)), localGraph, onComplete);
    }
    invokeNamedRuntimeMethod(object, method, parent, eventName,
                             retain(makeValue(eventArguments)));
    invokeCompletion(onComplete);
    return true;
}

void dispatchBlueprintEvent(const RuntimeValue& object,
                            const RuntimeValue& rawObjectType,
                            const std::string& eventName,
                            const RuntimeValue& rawKeywordArguments,
                            const std::function<void()>& onComplete) {
    const RuntimeValue isDestroyed =
        get(object, retain(makeValue("isDestroyed")));
    if (isFunction(isDestroyed) &&
        boolean(callRuntimeMethodFirst(object, "isDestroyed"))) {
        invokeCompletion(onComplete);
        return;
    }
    const RuntimeValue objectType =
        isTable(rawObjectType) ? rawObjectType : classType(object);
    if (!blueprintIsInstance(object, objectType)) {
        invokeCompletion(onComplete);
        return;
    }
    const RuntimeValue keywordArguments =
        isTable(rawKeywordArguments) ? rawKeywordArguments : table();
    const RuntimeValue rawClass = classType(object);
    const bool scriptMixin = isTable(rawClass) &&
                             is<bool>(get(rawClass, "scriptMixin")) &&
                             as<bool>(get(rawClass, "scriptMixin"));
    if (scriptMixin) {
        const RuntimeValue method = get(object, retain(makeValue(eventName)));
        invokeNamedRuntimeMethod(object, method, rawClass, eventName,
                                 retain(makeValue(keywordArguments)));
        invokeCompletion(onComplete);
        return;
    }
    const RuntimeValue actorType = blueprintEngineType("Actor");
    const RuntimeValue graph = blueprintIsInstance(object, actorType)
                                   ? callRuntimeMethodFirst(object, "getGraph")
                                   : RuntimeValue();
    const bool generated =
        isTable(rawClass) && boolean(rawGet(rawClass, "_GENERATED_CLASS"));
    if (generated && !graph.isNil()) {
        if (requireBlueprintGraph(graph)->hasKey(eventName)) {
            if (requireBlueprintGraph(graph)->startNodes.contains(eventName)) {
                if (!executeBlueprintGraph(graph, eventName,
                                           retain(makeValue(keywordArguments)),
                                           RuntimeValue(), onComplete)) {
                    invokeCompletion(onComplete);
                }
                return;
            }
        }
        if (executeParentBlueprintEvent(object, rawClass, eventName,
                                        RuntimeValue(),
                                        retain(makeValue(keywordArguments)),
                                        RuntimeValue(), onComplete)) {
            return;
        }
        const RuntimeValue method = get(object, retain(makeValue(eventName)));
        invokeNamedRuntimeMethod(object, method, rawClass, eventName,
                                 retain(makeValue(keywordArguments)));
        invokeCompletion(onComplete);
        return;
    }
    const RuntimeValue method = get(object, retain(makeValue(eventName)));
    invokeNamedRuntimeMethod(object, method, rawClass, eventName,
                             retain(makeValue(keywordArguments)));
    invokeCompletion(onComplete);
}

}  // namespace ludork::engine::runtime_detail
