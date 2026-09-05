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

RuntimeHandle createRuntimeParameterDescriptor(
    const std::vector<std::string>& names) {
    RuntimeHandle descriptor = table();
    RuntimeHandle parameters = table();
    RuntimeHandle accepted = table();
    std::size_t index = 1;
    for (const std::string& name : names) {
        rawSet(ludork::runtime::reference::intern(parameters), index++, name);
        rawSet(accepted, name, true);
    }
    rawSet(ludork::runtime::reference::intern(descriptor), "parameters",
           parameters);
    rawSet(ludork::runtime::reference::intern(descriptor), "accepted",
           accepted);
    return descriptor;
}

RuntimeHandle callableRuntimeParameterDescriptor(const RuntimeValue& method) {
    if (!isFunction(method)) {
        return createRuntimeParameterDescriptor(std::vector<std::string>{});
    }
    RuntimeHandle cache =
        registryTable(BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY, WeakMode::Keys);
    const RuntimeValue cached = rawGet(cache, method);
    if (isTable(cached)) {
        return intern(cached);
    }
    RuntimeHandle descriptor =
        createRuntimeParameterDescriptor(functionParameterNames(method));
    rawSet(cache, method, descriptor);
    return descriptor;
}

RuntimeHandle classRuntimeEventCache(const RuntimeValue& classType) {
    RuntimeHandle cache =
        registryTable(BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY, WeakMode::Keys);
    const RuntimeValue classObject = retain(makeValue(classType));
    const RuntimeValue cached = rawGet(cache, classObject);
    if (isTable(cached)) {
        return intern(cached);
    }
    RuntimeHandle result = table();
    rawSet(result, "members", table());
    rawSet(cache, classObject, result);
    return result;
}

RuntimeHandle buildRuntimeEventDescriptor(const RuntimeValue& classType,
                                          const std::string& eventName) {
    for (const RuntimeValue& current :
         classMro(ludork::runtime::reference::intern(classType))) {
        const RuntimeValue rawMetadata =
            typeMetadata(ludork::runtime::reference::intern(current));
        if (!isTable(rawMetadata)) {
            continue;
        }
        const RuntimeValue rawEvent =
            rawGet(ludork::runtime::reference::intern(rawMetadata), eventName);
        if (!isTable(rawEvent)) {
            continue;
        }
        std::vector<std::string> names;
        const RuntimeValue rawParameters =
            rawGet(ludork::runtime::reference::intern(rawEvent), "parameters");
        if (isTable(rawParameters)) {
            const RuntimeValue parameters = rawParameters;
            names.reserve(
                length(ludork::runtime::reference::intern(parameters)));
            for (std::size_t index = 1;
                 index <=
                 length(ludork::runtime::reference::intern(parameters));
                 ++index) {
                const RuntimeValue rawName = rawGet(
                    ludork::runtime::reference::intern(parameters), index);
                if (is<std::string>(rawName)) {
                    names.push_back(as<std::string>(rawName));
                }
            }
        }
        RuntimeHandle descriptor = createRuntimeParameterDescriptor(names);
        rawSet(ludork::runtime::reference::intern(descriptor), "metadataFound",
               true);
        rawSet(ludork::runtime::reference::intern(descriptor), "metadata",
               rawEvent);
        return descriptor;
    }

    const RuntimeValue classMethod =
        get(ludork::runtime::reference::intern(classType), eventName);
    RuntimeHandle descriptor = callableRuntimeParameterDescriptor(classMethod);
    rawSet(ludork::runtime::reference::intern(descriptor), "metadataFound",
           false);
    if (isFunction(classMethod)) {
        rawSet(ludork::runtime::reference::intern(descriptor), "sourceMethod",
               classMethod);
    }
    return descriptor;
}

RuntimeHandle runtimeEventDescriptor(const RuntimeValue& method,
                                     const RuntimeValue& classType,
                                     const std::string& eventName) {
    RuntimeHandle classCache = classRuntimeEventCache(classType);
    RuntimeHandle members = requireTable(rawGet(classCache, "members"));
    RuntimeValue rawDescriptor = rawGet(members, eventName);
    if (!isTable(rawDescriptor)) {
        RuntimeHandle descriptor =
            buildRuntimeEventDescriptor(classType, eventName);
        rawSet(members, eventName, descriptor);
        rawDescriptor = retain(makeValue(descriptor));
    }

    const RuntimeHandle descriptor = intern(rawDescriptor);
    const RuntimeValue rawMetadataFound =
        rawGet(ludork::runtime::reference::intern(descriptor), "metadataFound");
    if (is<bool>(rawMetadataFound) && as<bool>(rawMetadataFound)) {
        return descriptor;
    }
    const RuntimeValue sourceMethod =
        rawGet(ludork::runtime::reference::intern(descriptor), "sourceMethod");
    return rawEqual(sourceMethod, method)
               ? descriptor
               : callableRuntimeParameterDescriptor(method);
}

RuntimeHandle runtimeDescriptorParameters(const RuntimeValue& descriptor) {
    const RuntimeValue rawParameters =
        rawGet(ludork::runtime::reference::intern(descriptor), "parameters");
    return isTable(rawParameters) ? intern(rawParameters) : table();
}

RuntimeHandle runtimeDescriptorAccepted(const RuntimeValue& descriptor) {
    const RuntimeValue rawAccepted =
        rawGet(ludork::runtime::reference::intern(descriptor), "accepted");
    return isTable(rawAccepted) ? intern(rawAccepted) : table();
}

void invokeNamedRuntimeMethod(const RuntimeValue& object,
                              const RuntimeValue& method,
                              const RuntimeValue& classType,
                              const std::string& eventName,
                              const RuntimeValue& rawKeywordArguments) {
    if (!isFunction(method)) {
        return;
    }
    const RuntimeValue keywordArguments =
        isTable(rawKeywordArguments) ? rawKeywordArguments : table();
    const RuntimeHandle descriptor =
        runtimeEventDescriptor(method, classType, eventName);
    const RuntimeHandle names = runtimeDescriptorParameters(descriptor);
    const RuntimeHandle accepted = runtimeDescriptorAccepted(descriptor);
    for (const RuntimeValue& key :
         keys(ludork::runtime::reference::intern(
                  retain(makeValue(keywordArguments))),
              RuntimeLookupMode::Visible)) {
        if (!is<std::string>(key)) {
            continue;
        }
        const RuntimeValue acceptedValue =
            rawGet(accepted, as<std::string>(key));
        if (!is<bool>(acceptedValue) || !as<bool>(acceptedValue)) {
            throw std::invalid_argument(
                "Unexpected blueprint event argument '" + as<std::string>(key) +
                "'");
        }
    }
    RuntimeValue::Array arguments{object};
    arguments.reserve(length(names) + 1);
    for (std::size_t index = 1; index <= length(names); ++index) {
        const RuntimeValue rawName = rawGet(names, index);
        if (is<std::string>(rawName)) {
            arguments.push_back(get(
                ludork::runtime::reference::intern(keywordArguments), rawName));
        }
    }
    static_cast<void>(
        invoke(ludork::runtime::reference::intern(method), arguments));
}

bool calculateRuntimeMethodHasImplementation(const RuntimeValue& method) {
    if (!isFunction(method)) {
        return false;
    }
    const std::optional<FunctionSource> source = functionSource(method);
    if (!source.has_value()) {
        return true;
    }
    std::ifstream file(ludork::standard::pathFromUtf8(source->path));
    if (!file) {
        return true;
    }
    const std::size_t firstLine = source->firstLine;
    const std::size_t lastLine = source->lastLine;
    std::string text;
    std::string line;
    std::size_t lineIndex = 0;
    while (std::getline(file, line)) {
        ++lineIndex;
        if (lineIndex >= firstLine && lineIndex <= lastLine) {
            const std::size_t comment = line.find("--");
            if (comment != std::string::npos) {
                line.erase(comment);
            }
            text += line;
            text.push_back('\n');
        }
        if (lineIndex > lastLine) {
            break;
        }
    }
    text = std::regex_replace(
        text, std::regex(R"(^\s*(?:local\s+)?function[^\n\(]*\([^\)]*\)\s*)"),
        std::string());
    text = std::regex_replace(text, std::regex(R"(\s*end\s*$)"), std::string());
    text = std::regex_replace(text, std::regex(R"(\s+)"), std::string(" "));
    const std::size_t start = text.find_first_not_of(' ');
    if (start == std::string::npos) {
        return false;
    }
    const std::size_t end = text.find_last_not_of(' ');
    const std::string body = text.substr(start, end - start + 1);
    return !body.empty() && body != "return" && body != "return nil";
}

bool runtimeMethodHasImplementation(const RuntimeValue& method) {
    if (!isFunction(method)) {
        return false;
    }
    RuntimeHandle cache =
        registryTable(BLUEPRINT_IMPLEMENTATION_CACHE_KEY, WeakMode::Keys);
    const RuntimeValue cached = rawGet(cache, method);
    if (is<bool>(cached)) {
        return as<bool>(cached);
    }
    const bool result = calculateRuntimeMethodHasImplementation(method);
    rawSet(cache, method, result);
    return result;
}

}  // namespace ludork::runtime::blueprint_detail
