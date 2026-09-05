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

RuntimeValue createRuntimeParameterDescriptor(
    const std::vector<std::string>& names) {
    RuntimeValue descriptor = table();
    RuntimeValue parameters = table();
    RuntimeValue accepted = table();
    std::size_t index = 1;
    for (const std::string& name : names) {
        rawSet(parameters, index++, name);
        rawSet(accepted, name, true);
    }
    rawSet(descriptor, "parameters", parameters);
    rawSet(descriptor, "accepted", accepted);
    return descriptor;
}

RuntimeValue callableRuntimeParameterDescriptor(const RuntimeValue& method) {
    if (!isFunction(method)) {
        return createRuntimeParameterDescriptor(std::vector<std::string>{});
    }
    RuntimeValue cache =
        registryTable(BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY, WeakMode::Keys);
    const RuntimeValue cached = rawGet(cache, method);
    if (isTable(cached)) {
        return cached;
    }
    RuntimeValue descriptor =
        createRuntimeParameterDescriptor(functionParameterNames(method));
    rawSet(cache, method, descriptor);
    return descriptor;
}

RuntimeValue classRuntimeEventCache(const RuntimeValue& classType) {
    RuntimeValue cache =
        registryTable(BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY, WeakMode::Keys);
    const RuntimeValue classObject = retain(makeValue(classType));
    const RuntimeValue cached = rawGet(cache, classObject);
    if (isTable(cached)) {
        return cached;
    }
    RuntimeValue result = table();
    rawSet(result, "members", table());
    rawSet(cache, classObject, result);
    return result;
}

RuntimeValue buildRuntimeEventDescriptor(const RuntimeValue& classType,
                                         const std::string& eventName) {
    for (const RuntimeValue& current : classMro(classType)) {
        const RuntimeValue rawMetadata = typeMetadata(current);
        if (!isTable(rawMetadata)) {
            continue;
        }
        const RuntimeValue rawEvent = rawGet(rawMetadata, eventName);
        if (!isTable(rawEvent)) {
            continue;
        }
        std::vector<std::string> names;
        const RuntimeValue rawParameters = rawGet(rawEvent, "parameters");
        if (isTable(rawParameters)) {
            const RuntimeValue parameters = rawParameters;
            names.reserve(length(parameters));
            for (std::size_t index = 1; index <= length(parameters); ++index) {
                const RuntimeValue rawName = rawGet(parameters, index);
                if (is<std::string>(rawName)) {
                    names.push_back(as<std::string>(rawName));
                }
            }
        }
        RuntimeValue descriptor = createRuntimeParameterDescriptor(names);
        rawSet(descriptor, "metadataFound", true);
        rawSet(descriptor, "metadata", rawEvent);
        return descriptor;
    }

    const RuntimeValue classMethod = get(classType, eventName);
    RuntimeValue descriptor = callableRuntimeParameterDescriptor(classMethod);
    rawSet(descriptor, "metadataFound", false);
    if (isFunction(classMethod)) {
        rawSet(descriptor, "sourceMethod", classMethod);
    }
    return descriptor;
}

RuntimeValue runtimeEventDescriptor(const RuntimeValue& method,
                                    const RuntimeValue& classType,
                                    const std::string& eventName) {
    RuntimeValue classCache = classRuntimeEventCache(classType);
    RuntimeValue members = requireTable(rawGet(classCache, "members"));
    RuntimeValue rawDescriptor = rawGet(members, eventName);
    if (!isTable(rawDescriptor)) {
        RuntimeValue descriptor =
            buildRuntimeEventDescriptor(classType, eventName);
        rawSet(members, eventName, descriptor);
        rawDescriptor = retain(makeValue(descriptor));
    }

    const RuntimeValue descriptor = rawDescriptor;
    const RuntimeValue rawMetadataFound = rawGet(descriptor, "metadataFound");
    if (is<bool>(rawMetadataFound) && as<bool>(rawMetadataFound)) {
        return descriptor;
    }
    const RuntimeValue sourceMethod = rawGet(descriptor, "sourceMethod");
    return rawEqual(sourceMethod, method)
               ? descriptor
               : callableRuntimeParameterDescriptor(method);
}

RuntimeValue runtimeDescriptorParameters(const RuntimeValue& descriptor) {
    const RuntimeValue rawParameters = rawGet(descriptor, "parameters");
    return isTable(rawParameters) ? rawParameters : table();
}

RuntimeValue runtimeDescriptorAccepted(const RuntimeValue& descriptor) {
    const RuntimeValue rawAccepted = rawGet(descriptor, "accepted");
    return isTable(rawAccepted) ? rawAccepted : table();
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
    const RuntimeValue descriptor =
        runtimeEventDescriptor(method, classType, eventName);
    const RuntimeValue names = runtimeDescriptorParameters(descriptor);
    const RuntimeValue accepted = runtimeDescriptorAccepted(descriptor);
    for (const RuntimeValue& key : keys(retain(makeValue(keywordArguments)),
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
            arguments.push_back(get(keywordArguments, rawName));
        }
    }
    static_cast<void>(invoke(method, arguments));
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
    RuntimeValue cache =
        registryTable(BLUEPRINT_IMPLEMENTATION_CACHE_KEY, WeakMode::Keys);
    const RuntimeValue cached = rawGet(cache, method);
    if (is<bool>(cached)) {
        return as<bool>(cached);
    }
    const bool result = calculateRuntimeMethodHasImplementation(method);
    rawSet(cache, method, result);
    return result;
}

}  // namespace ludork::engine::runtime_detail
