#include <Runtime/RuntimeReference.hpp>
#include "BlueprintRuntimeInternal.hpp"

#include <Runtime/Components/ComponentRuntime.hpp>
#include <Runtime/NodeGraph/Graph.hpp>
#include <Runtime/Blueprint/ClassRuntime.hpp>
#include <RuntimeSession.hpp>
#include <Utf8Path.hpp>
#include <Runtime/TypedDataService.hpp>

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

namespace ludork::runtime::blueprint_detail {

using namespace ludork::runtime::reference;

std::function<void()> completionCallback(const RuntimeValue& value) {
    if (value.isNil()) {
        return {};
    }
    if (!isFunction(value)) {
        throw std::invalid_argument("Blueprint completion must be a function");
    }
    return [value] {
        static_cast<void>(invoke(ludork::runtime::reference::intern(value)));
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
    dispatchBlueprintEvent(object, classType(object), eventName, table(), {});
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
    const RuntimeValue generated = rawGet(
        ludork::runtime::reference::intern(classType), "_GENERATED_CLASS");
    if (boolean(generated)) {
        const RuntimeValue rawScriptMixin =
            get(ludork::runtime::reference::intern(classType), "scriptMixin");
        if (is<bool>(rawScriptMixin) && as<bool>(rawScriptMixin)) {
            const RuntimeValue method = rawGet(
                ludork::runtime::reference::intern(classType), eventName);
            if (runtimeMethodHasImplementation(method)) {
                return true;
            }
            return classHasBlueprintEvent(
                rawGet(ludork::runtime::reference::intern(classType), "__base"),
                eventName);
        }
        if (generatedBlueprintGraphHasExecutableEvent(classType, eventName)) {
            return true;
        }
        return classHasBlueprintEvent(
            rawGet(ludork::runtime::reference::intern(classType), "__base"),
            eventName);
    }
    const RuntimeValue graph =
        rawGet(ludork::runtime::reference::intern(classType), "_graph");
    if (blueprintGraphHasExecutableEvent(graph, eventName)) {
        return true;
    }
    const RuntimeValue method =
        rawGet(ludork::runtime::reference::intern(classType), eventName);
    if (isClass(classType) && runtimeMethodHasImplementation(method)) {
        return true;
    }
    return classHasBlueprintEvent(
        rawGet(ludork::runtime::reference::intern(classType), "__base"),
        eventName);
}

bool hasBlueprintEvent(const RuntimeValue& object,
                       const std::string& eventName) {
    if (object.isNil() || eventName.empty()) {
        return false;
    }
    const RuntimeValue rawClass = classType(object);
    const bool scriptMixin =
        isTable(rawClass) &&
        is<bool>(
            get(ludork::runtime::reference::intern(rawClass), "scriptMixin")) &&
        as<bool>(
            get(ludork::runtime::reference::intern(rawClass), "scriptMixin"));
    const RuntimeValue actorGraph =
        !scriptMixin ? objectGraph(object) : RuntimeValue();
    if (blueprintGraphHasExecutableEvent(actorGraph, eventName)) {
        return true;
    }
    RuntimeValue instanceMethod = RuntimeValue();
    instanceMethod =
        rawGet(ludork::runtime::reference::intern(object), eventName);
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
    const RuntimeValue rawParent =
        rawGet(ludork::runtime::reference::intern(rawClass), "__base");
    if (!isTable(rawParent)) {
        return false;
    }
    const RuntimeValue parent = rawParent;
    RuntimeValue eventArguments = blueprintEventKeywordArguments(
        parent, eventName, arguments, keywordArguments);
    mergeBlueprintLocalArguments(parent, eventName, eventArguments, localGraph);
    if (boolean(rawGet(ludork::runtime::reference::intern(parent),
                       "_GENERATED_CLASS"))) {
        if (generatedBlueprintGraphHasExecutableEvent(parent, eventName)) {
            const RuntimeValue graph = generatedBlueprintGraph(object, parent);
            if (!graph.isNil()) {
                if (!executeBlueprintGraph(graph, eventName, eventArguments,
                                           localGraph, onComplete)) {
                    invokeCompletion(onComplete);
                }
                return true;
            }
        }
        return executeParentBlueprintEvent(object, rawParent, eventName,
                                           RuntimeValue(), eventArguments,
                                           localGraph, onComplete);
    }

    const RuntimeValue graph =
        get(ludork::runtime::reference::intern(parent), "_graph");
    if (!graph.isNil() && requireBlueprintGraph(graph)->hasKey(eventName)) {
        if (requireBlueprintGraph(graph)->startNodes.contains(eventName)) {
            if (!executeBlueprintGraph(graph, eventName, eventArguments,
                                       localGraph, onComplete)) {
                invokeCompletion(onComplete);
            }
            return true;
        }
        return executeParentBlueprintEvent(object, rawParent, eventName,
                                           RuntimeValue(), eventArguments,
                                           localGraph, onComplete);
    }

    const RuntimeValue method =
        get(ludork::runtime::reference::intern(parent), eventName);
    if (!isFunction(method)) {
        return executeParentBlueprintEvent(object, rawParent, eventName,
                                           RuntimeValue(), eventArguments,
                                           localGraph, onComplete);
    }
    invokeNamedRuntimeMethod(object, method, parent, eventName, eventArguments);
    invokeCompletion(onComplete);
    return true;
}

void dispatchBlueprintEvent(const RuntimeValue& object,
                            const RuntimeValue& rawObjectType,
                            const std::string& eventName,
                            const RuntimeValue& rawKeywordArguments,
                            const std::function<void()>& onComplete) {
    const RuntimeValue isDestroyed =
        get(ludork::runtime::reference::intern(object), "isDestroyed");
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
        isTable(rawKeywordArguments) ? intern(rawKeywordArguments) : table();
    const RuntimeValue rawClass = classType(object);
    const bool scriptMixin =
        isTable(rawClass) &&
        is<bool>(
            get(ludork::runtime::reference::intern(rawClass), "scriptMixin")) &&
        as<bool>(
            get(ludork::runtime::reference::intern(rawClass), "scriptMixin"));
    if (scriptMixin) {
        const RuntimeValue method =
            get(ludork::runtime::reference::intern(object), eventName);
        invokeNamedRuntimeMethod(object, method, rawClass, eventName,
                                 keywordArguments);
        invokeCompletion(onComplete);
        return;
    }
    const RuntimeValue graph = objectGraph(object);
    const bool generated =
        isTable(rawClass) &&
        boolean(rawGet(ludork::runtime::reference::intern(rawClass),
                       "_GENERATED_CLASS"));
    if (generated && !graph.isNil()) {
        if (requireBlueprintGraph(graph)->hasKey(eventName)) {
            if (requireBlueprintGraph(graph)->startNodes.contains(eventName)) {
                if (!executeBlueprintGraph(graph, eventName, keywordArguments,
                                           RuntimeValue(), onComplete)) {
                    invokeCompletion(onComplete);
                }
                return;
            }
        }
        if (executeParentBlueprintEvent(object, rawClass, eventName,
                                        RuntimeValue(), keywordArguments,
                                        RuntimeValue(), onComplete)) {
            return;
        }
        const RuntimeValue method =
            get(ludork::runtime::reference::intern(object), eventName);
        invokeNamedRuntimeMethod(object, method, rawClass, eventName,
                                 keywordArguments);
        invokeCompletion(onComplete);
        return;
    }
    const RuntimeValue method =
        get(ludork::runtime::reference::intern(object), eventName);
    invokeNamedRuntimeMethod(object, method, rawClass, eventName,
                             keywordArguments);
    invokeCompletion(onComplete);
}

}  // namespace ludork::runtime::blueprint_detail
