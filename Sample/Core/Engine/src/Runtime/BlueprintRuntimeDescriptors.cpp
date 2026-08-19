#include "BlueprintRuntimeInternal.hpp"
#include <Runtime/EngineRuntimeServices.hpp>

#include <ClassServices.hpp>
#include <Gameplay/Components/ComponentRuntime.hpp>
#include <NodeGraph/Graph.hpp>
#include <Runtime/EngineClassRuntime.hpp>
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
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::engine::runtime_detail {

std::vector<std::string> debugRuntimeMethodParameterNames(
    sol::state_view lua, const sol::object& method) {
    std::vector<std::string> names;
    if (!method.is<sol::protected_function>()) {
        return names;
    }
    const sol::object rawDebug = lua.globals().raw_get<sol::object>("debug");
    if (!rawDebug.is<sol::table>()) {
        return names;
    }
    const sol::table debug = rawDebug.as<sol::table>();
    const sol::object rawGetInfo = debug.raw_get<sol::object>("getinfo");
    const sol::object rawGetLocal = debug.raw_get<sol::object>("getlocal");
    if (!rawGetInfo.is<sol::protected_function>() ||
        !rawGetLocal.is<sol::protected_function>()) {
        return names;
    }
    sol::protected_function getInfo = rawGetInfo.as<sol::protected_function>();
    sol::protected_function_result infoResult = getInfo(method, "u");
    const sol::object rawInfo = checkedResult(lua, infoResult);
    if (!rawInfo.is<sol::table>()) {
        return names;
    }
    const sol::object rawCount =
        rawInfo.as<sol::table>().raw_get<sol::object>("nparams");
    const std::size_t count =
        rawCount.is<std::size_t>() ? rawCount.as<std::size_t>() : 0;
    sol::protected_function getLocal =
        rawGetLocal.as<sol::protected_function>();
    names.reserve(count);
    for (std::size_t index = 1; index <= count; ++index) {
        sol::protected_function_result localResult = getLocal(method, index);
        if (!localResult.valid()) {
            const sol::error error = localResult;
            throw std::runtime_error(error.what());
        }
        if (localResult.return_count() == 0) {
            continue;
        }
        const sol::object rawName = localResult.get<sol::object>(0);
        if (!rawName.is<std::string>()) {
            continue;
        }
        const std::string name = rawName.as<std::string>();
        if (name != "self") {
            names.push_back(name);
        }
    }
    return names;
}

sol::table createRuntimeParameterDescriptor(
    sol::state_view lua, const std::vector<std::string>& names) {
    sol::table descriptor = lua.create_table();
    sol::table parameters = lua.create_table(static_cast<int>(names.size()), 0);
    sol::table accepted = lua.create_table(0, static_cast<int>(names.size()));
    std::size_t index = 1;
    for (const std::string& name : names) {
        parameters.raw_set(index++, name);
        accepted.raw_set(name, true);
    }
    descriptor.raw_set("parameters", parameters);
    descriptor.raw_set("accepted", accepted);
    return descriptor;
}

sol::table callableRuntimeParameterDescriptor(sol::state_view lua,
                                              const sol::object& method) {
    if (!method.is<sol::protected_function>()) {
        return createRuntimeParameterDescriptor(lua,
                                                std::vector<std::string>{});
    }
    sol::table cache =
        registryTable(lua, BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY, "k");
    const sol::object cached = cache.raw_get<sol::object>(method);
    if (cached.is<sol::table>()) {
        return cached.as<sol::table>();
    }
    sol::table descriptor = createRuntimeParameterDescriptor(
        lua, debugRuntimeMethodParameterNames(lua, method));
    cache.raw_set(method, descriptor);
    return descriptor;
}

sol::table classRuntimeEventCache(sol::state_view lua,
                                  const sol::table& classType) {
    sol::table cache =
        registryTable(lua, BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY, "k");
    const sol::object classObject = sol::make_object(lua, classType);
    const sol::object cached = cache.raw_get<sol::object>(classObject);
    if (cached.is<sol::table>()) {
        return cached.as<sol::table>();
    }
    sol::table result = lua.create_table();
    result.raw_set("members", lua.create_table());
    cache.raw_set(classObject, result);
    return result;
}

sol::table buildRuntimeEventDescriptor(sol::state_view lua,
                                       const sol::table& classType,
                                       const std::string& eventName) {
    for (const sol::table& current : runtimeClassMro(lua, classType)) {
        const sol::object rawMetadata = runtimeTypeMetadata(lua, current);
        if (!rawMetadata.is<sol::table>()) {
            continue;
        }
        const sol::object rawEvent =
            rawMetadata.as<sol::table>().raw_get<sol::object>(eventName);
        if (!rawEvent.is<sol::table>()) {
            continue;
        }
        std::vector<std::string> names;
        const sol::object rawParameters =
            rawEvent.as<sol::table>().raw_get<sol::object>("parameters");
        if (rawParameters.is<sol::table>()) {
            const sol::table parameters = rawParameters.as<sol::table>();
            names.reserve(parameters.size());
            for (std::size_t index = 1; index <= parameters.size(); ++index) {
                const sol::object rawName =
                    parameters.raw_get<sol::object>(index);
                if (rawName.is<std::string>()) {
                    names.push_back(rawName.as<std::string>());
                }
            }
        }
        sol::table descriptor = createRuntimeParameterDescriptor(lua, names);
        descriptor.raw_set("metadataFound", true);
        descriptor.raw_set("metadata", rawEvent);
        return descriptor;
    }

    const sol::object classMethod = classType.get<sol::object>(eventName);
    sol::table descriptor =
        callableRuntimeParameterDescriptor(lua, classMethod);
    descriptor.raw_set("metadataFound", false);
    if (classMethod.is<sol::protected_function>()) {
        descriptor.raw_set("sourceMethod", classMethod);
    }
    return descriptor;
}

sol::table runtimeEventDescriptor(sol::state_view lua,
                                  const sol::object& method,
                                  const sol::table& classType,
                                  const std::string& eventName) {
    sol::table classCache = classRuntimeEventCache(lua, classType);
    sol::table members = classCache.raw_get<sol::table>("members");
    sol::object rawDescriptor = members.raw_get<sol::object>(eventName);
    if (!rawDescriptor.is<sol::table>()) {
        sol::table descriptor =
            buildRuntimeEventDescriptor(lua, classType, eventName);
        members.raw_set(eventName, descriptor);
        rawDescriptor = sol::make_object(lua, descriptor);
    }

    const sol::table descriptor = rawDescriptor.as<sol::table>();
    const sol::object rawMetadataFound =
        descriptor.raw_get<sol::object>("metadataFound");
    if (rawMetadataFound.is<bool>() && rawMetadataFound.as<bool>()) {
        return descriptor;
    }
    const sol::object sourceMethod =
        descriptor.raw_get<sol::object>("sourceMethod");
    return rawEqual(lua, sourceMethod, method)
               ? descriptor
               : callableRuntimeParameterDescriptor(lua, method);
}

sol::table runtimeDescriptorParameters(sol::state_view lua,
                                       const sol::table& descriptor) {
    const sol::object rawParameters =
        descriptor.raw_get<sol::object>("parameters");
    return rawParameters.is<sol::table>() ? rawParameters.as<sol::table>()
                                          : lua.create_table();
}

sol::table runtimeDescriptorAccepted(sol::state_view lua,
                                     const sol::table& descriptor) {
    const sol::object rawAccepted = descriptor.raw_get<sol::object>("accepted");
    return rawAccepted.is<sol::table>() ? rawAccepted.as<sol::table>()
                                        : lua.create_table();
}

void invokeNamedRuntimeMethod(sol::state_view lua, const sol::object& object,
                              const sol::object& method,
                              const sol::table& classType,
                              const std::string& eventName,
                              const sol::object& rawKeywordArguments) {
    if (!method.is<sol::protected_function>()) {
        return;
    }
    const sol::table keywordArguments =
        rawKeywordArguments.is<sol::table>()
            ? rawKeywordArguments.as<sol::table>()
            : lua.create_table();
    const sol::table descriptor =
        runtimeEventDescriptor(lua, method, classType, eventName);
    const sol::table names = runtimeDescriptorParameters(lua, descriptor);
    const sol::table accepted = runtimeDescriptorAccepted(lua, descriptor);
    for (const sol::object& key :
         runtimeKeys(lua, sol::make_object(lua, keywordArguments), false)) {
        if (!key.is<std::string>()) {
            continue;
        }
        const sol::object acceptedValue =
            accepted.raw_get<sol::object>(key.as<std::string>());
        if (!acceptedValue.is<bool>() || !acceptedValue.as<bool>()) {
            throw std::invalid_argument(
                "Unexpected blueprint event argument '" +
                key.as<std::string>() + "'");
        }
    }
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    try {
        if (names.size() > static_cast<std::size_t>(INT_MAX - 1)) {
            throw std::length_error("Blueprint event argument count overflow");
        }
        std::vector<sol::object> arguments;
        arguments.reserve(names.size() + 1);
        arguments.push_back(object);
        for (std::size_t index = 1; index <= names.size(); ++index) {
            const sol::object rawName = names.raw_get<sol::object>(index);
            if (rawName.is<std::string>()) {
                arguments.push_back(keywordArguments.get<sol::object>(
                    rawName.as<std::string>()));
            }
        }
        static_cast<void>(invokeRuntimeFunction(state, method, arguments,
                                                "blueprint event arguments"));
        lua_settop(state, stackBase);
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

bool calculateRuntimeMethodHasImplementation(sol::state_view lua,
                                             const sol::object& method) {
    if (!method.is<sol::protected_function>()) {
        return false;
    }
    const sol::object rawDebug = lua.globals().raw_get<sol::object>("debug");
    if (!rawDebug.is<sol::table>()) {
        return true;
    }
    const sol::object rawGetInfo =
        rawDebug.as<sol::table>().raw_get<sol::object>("getinfo");
    if (!rawGetInfo.is<sol::protected_function>()) {
        return true;
    }
    sol::protected_function getInfo = rawGetInfo.as<sol::protected_function>();
    sol::protected_function_result infoResult = getInfo(method, "S");
    const sol::object rawInfo = checkedResult(lua, infoResult);
    if (!rawInfo.is<sol::table>()) {
        return true;
    }
    const sol::table info = rawInfo.as<sol::table>();
    const sol::object rawWhat = info.raw_get<sol::object>("what");
    const sol::object rawSource = info.raw_get<sol::object>("source");
    if (!rawWhat.is<std::string>() || rawWhat.as<std::string>() != "Lua" ||
        !rawSource.is<std::string>()) {
        return true;
    }
    const std::string source = rawSource.as<std::string>();
    if (source.empty() || source.front() != '@') {
        return true;
    }
    const sol::object rawFirst = info.raw_get<sol::object>("linedefined");
    const sol::object rawLast = info.raw_get<sol::object>("lastlinedefined");
    if (!rawFirst.is<std::size_t>() || !rawLast.is<std::size_t>()) {
        return true;
    }
    std::ifstream file(ludork::standard::pathFromUtf8(source.substr(1)));
    if (!file) {
        return true;
    }
    const std::size_t firstLine = rawFirst.as<std::size_t>();
    const std::size_t lastLine = rawLast.as<std::size_t>();
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

bool runtimeMethodHasImplementation(sol::state_view lua,
                                    const sol::object& method) {
    if (!method.is<sol::protected_function>()) {
        return false;
    }
    sol::table cache =
        registryTable(lua, BLUEPRINT_IMPLEMENTATION_CACHE_KEY, "k");
    const sol::object cached = cache.raw_get<sol::object>(method);
    if (cached.is<bool>()) {
        return cached.as<bool>();
    }
    const bool result = calculateRuntimeMethodHasImplementation(lua, method);
    cache.raw_set(method, result);
    return result;
}

}  // namespace ludork::engine::runtime_detail
