#include <Runtime/RuntimeReference.hpp>
#include <ClassRuntimeProtocol.hpp>
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
    dispatchBlueprintEvent(object, classType(object), eventName, {}, {});
}

namespace {

bool dispatchUsesScriptMixin(const RuntimeHandle& type) {
    const RuntimeValue own = rawGet(type, "scriptMixin");
    if (!own.isNil()) {
        return boolean(own);
    }
    // An inherited lookup may execute a getter on the class metatable.
    return is<bool>(get(type, "scriptMixin")) &&
           as<bool>(get(type, "scriptMixin"));
}

std::shared_ptr<Graph> objectEventGraph(const RuntimeValue& object,
                                        const RuntimeValue& rawClass) {
    const RuntimeValue rawScriptMixin =
        isTable(rawClass) ? get(intern(rawClass), "scriptMixin")
                          : RuntimeValue();
    const bool scriptMixin =
        is<bool>(rawScriptMixin) && as<bool>(rawScriptMixin);
    return !scriptMixin ? objectGraph(object) : nullptr;
}

bool hasObjectBlueprintEvent(const RuntimeValue& object,
                             const RuntimeValue& rawClass,
                             const std::shared_ptr<Graph>& actorGraph,
                             const std::string& eventName,
                             const RuntimeHandle& classEventCache) {
    if (eventName.empty()) {
        return false;
    }
    if (blueprintGraphHasExecutableEvent(actorGraph, eventName)) {
        return true;
    }
    const RuntimeValue instanceMethod = rawGet(intern(object), eventName);
    if (runtimeMethodHasImplementation(instanceMethod)) {
        return true;
    }
    return classHasBlueprintEvent(rawClass, eventName, classEventCache);
}

bool calculateClassHasBlueprintEvent(const RuntimeValue& rawClass,
                                     const std::string& eventName,
                                     const RuntimeHandle& classEventCache) {
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
                rawGet(ludork::runtime::reference::intern(classType),
                       ludork::standard::class_runtime::protocol::
                           CLASS_BASE_FIELD),
                eventName, classEventCache);
        }
        if (generatedBlueprintGraphHasExecutableEvent(classType, eventName)) {
            return true;
        }
        return classHasBlueprintEvent(
            rawGet(ludork::runtime::reference::intern(classType),
                   ludork::standard::class_runtime::protocol::CLASS_BASE_FIELD),
            eventName, classEventCache);
    }
    const RuntimeValue graph =
        rawGet(ludork::runtime::reference::intern(classType), "_graph");
    if (blueprintGraphHasExecutableEvent(
            graph.isNil() ? nullptr : requireBlueprintGraph(graph),
            eventName)) {
        return true;
    }
    const RuntimeValue method =
        rawGet(ludork::runtime::reference::intern(classType), eventName);
    if (isClass(classType) && runtimeMethodHasImplementation(method)) {
        return true;
    }
    return classHasBlueprintEvent(
        rawGet(ludork::runtime::reference::intern(classType),
               ludork::standard::class_runtime::protocol::CLASS_BASE_FIELD),
        eventName, classEventCache);
}

}  // namespace

bool classHasBlueprintEvent(const RuntimeValue& rawClass,
                            const std::string& eventName,
                            const RuntimeHandle& classEventCache) {
    if (classEventCache.isNil() || !isTable(rawClass)) {
        return calculateClassHasBlueprintEvent(rawClass, eventName,
                                               classEventCache);
    }
    RuntimeValue classEvents = rawGet(classEventCache, rawClass);
    if (classEvents.isNil()) {
        classEvents = table();
        rawSet(classEventCache, rawClass, classEvents);
    }
    const RuntimeHandle events = intern(classEvents);
    const RuntimeValue cached = rawGet(events, eventName);
    if (is<bool>(cached)) {
        return as<bool>(cached);
    }
    const bool result =
        calculateClassHasBlueprintEvent(rawClass, eventName, classEventCache);
    rawSet(events, eventName, result);
    return result;
}

bool hasBlueprintEvent(const RuntimeValue& object, const std::string& eventName,
                       const RuntimeHandle& classEventCache) {
    if (object.isNil() || eventName.empty()) {
        return false;
    }
    const RuntimeValue rawClass = classType(object);
    const std::shared_ptr<Graph> actorGraph =
        objectEventGraph(object, rawClass);
    return hasObjectBlueprintEvent(object, rawClass, actorGraph, eventName,
                                   classEventCache);
}

std::vector<bool> hasBlueprintEvents(const RuntimeValue& object,
                                     const std::vector<std::string>& eventNames,
                                     const RuntimeHandle& classEventCache) {
    std::vector<bool> result(eventNames.size(), false);
    if (object.isNil() || eventNames.empty()) {
        return result;
    }
    const RuntimeValue rawClass = classType(object);
    const std::shared_ptr<Graph> actorGraph =
        objectEventGraph(object, rawClass);
    for (std::size_t index = 0; index < eventNames.size(); ++index) {
        result[index] = hasObjectBlueprintEvent(
            object, rawClass, actorGraph, eventNames[index], classEventCache);
    }
    return result;
}

bool executeParentBlueprintEvent(const RuntimeValue& object,
                                 const RuntimeValue& rawClass,
                                 const std::string& eventName,
                                 const RuntimeValue& arguments,
                                 const EventArguments& keywordArguments,
                                 const RuntimeValue& localGraph,
                                 const std::function<void()>& onComplete) {
    if (!isTable(rawClass)) {
        return false;
    }
    const RuntimeValue rawParent =
        rawGet(ludork::runtime::reference::intern(rawClass),
               ludork::standard::class_runtime::protocol::CLASS_BASE_FIELD);
    if (!isTable(rawParent)) {
        return false;
    }
    const RuntimeValue parent = rawParent;
    EventArguments eventArguments =
        blueprintEventArguments(parent, eventName, arguments, keywordArguments);
    mergeBlueprintLocalArguments(parent, eventName, eventArguments, localGraph);
    if (boolean(rawGet(ludork::runtime::reference::intern(parent),
                       "_GENERATED_CLASS"))) {
        if (generatedBlueprintGraphHasExecutableEvent(parent, eventName)) {
            const std::shared_ptr<Graph> graph =
                generatedBlueprintGraph(object, parent);
            if (graph != nullptr) {
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

    const RuntimeValue rawGraph = get(intern(parent), "_graph");
    const std::shared_ptr<Graph> graph =
        rawGraph.isNil() ? nullptr : requireBlueprintGraph(rawGraph);
    if (graph != nullptr && graph->hasKey(eventName)) {
        if (graph->startNodes.contains(eventName)) {
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
                            const EventArguments& keywordArguments,
                            const std::function<void()>& onComplete,
                            const RuntimeHandle& keywordSource) {
    const RuntimeHandle receiver = intern(object);
    if (blueprintObjectIsDestroyed(object)) {
        invokeCompletion(onComplete);
        return;
    }
    const bool explicitType = !rawObjectType.isNil() && isTable(rawObjectType);
    const RuntimeValue objectType =
        explicitType ? rawObjectType : classType(object);
    // Validating a native type can call a Lua predicate that changes the
    // receiver.
    const bool reuseObjectType = !explicitType && isClass(objectType);
    if (!isInstance(object, objectType)) {
        invokeCompletion(onComplete);
        return;
    }
    const RuntimeValue rawClass =
        reuseObjectType ? objectType : classType(object);
    const bool classIsTable = isTable(rawClass);
    const bool scriptMixin =
        classIsTable && dispatchUsesScriptMixin(intern(rawClass));
    if (scriptMixin) {
        const RuntimeValue method = get(receiver, eventName);
        invokeNamedRuntimeMethod(object, method, rawClass, eventName,
                                 keywordArguments, keywordSource);
        invokeCompletion(onComplete);
        return;
    }
    const std::shared_ptr<Graph> graph = objectGraph(object);
    const bool generated =
        classIsTable &&
        boolean(rawGet(ludork::runtime::reference::intern(rawClass),
                       "_GENERATED_CLASS"));
    if (generated && graph != nullptr) {
        if (graph->hasKey(eventName)) {
            if (graph->startNodes.contains(eventName)) {
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
    }
    const RuntimeValue method = get(receiver, eventName);
    invokeNamedRuntimeMethod(object, method, rawClass, eventName,
                             keywordArguments, keywordSource);
    invokeCompletion(onComplete);
}

}  // namespace ludork::runtime::blueprint_detail
