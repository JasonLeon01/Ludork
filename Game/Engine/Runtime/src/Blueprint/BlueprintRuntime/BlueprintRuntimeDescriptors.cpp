#include <Runtime/RuntimeReference.hpp>
#include "BlueprintRuntimeInternal.hpp"

#include <Runtime/Components/ComponentRuntime.hpp>
#include <Runtime/NodeGraph/Graph.hpp>
#include <Runtime/Blueprint/ClassRuntime.hpp>
#include <RuntimeSession.hpp>
#include <Utf8Path.hpp>
#include <Runtime/TypedDataService.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include "LuaServices/RuntimeBindingTraits.hpp"
#include "LuaServices/RuntimeReferenceConversion.hpp"

extern "C" {
#include <lauxlib.h>
}

#include <algorithm>
#include <climits>
#include <cstddef>
#include <fstream>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::runtime::blueprint_detail {

using namespace ludork::runtime::reference;

namespace {

constexpr const char* descriptorMetatable = "Ludork.Runtime.EventDescriptor";
using DescriptorOwner = std::shared_ptr<const EventDescriptor>;

int destroyDescriptor(lua_State* state) {
    auto* owner = static_cast<DescriptorOwner*>(
        luaL_checkudata(state, 1, descriptorMetatable));
    owner->~shared_ptr();
    return 0;
}

RuntimeValue storeDescriptor(const DescriptorOwner& descriptor,
                             const RuntimeValue& sourceMethod) {
    RuntimeScope scope;
    lua_State* state = scope.state();
    lua_glue::StateView lua(state);
    const lua_glue::Object method = binding::writeLuaValue(lua, sourceMethod);
    if (luaL_newmetatable(state, descriptorMetatable)) {
        lua_pushcfunction(state, destroyDescriptor);
        lua_setfield(state, -2, "__gc");
    }
    lua_pop(state, 1);
    new (lua_newuserdatauv(state, sizeof(DescriptorOwner), 1))
        DescriptorOwner(descriptor);
    luaL_setmetatable(state, descriptorMetatable);
    method.push(state);
    lua_setiuservalue(state, -2, 1);
    const lua_glue::Object stored = lua_glue::Read<lua_glue::Object>(state, -1);
    lua_pop(state, 1);
    return detail::readRuntimeReference(stored);
}

DescriptorOwner loadDescriptor(const RuntimeValue& value,
                               const RuntimeValue* sourceMethod = nullptr) {
    if (value.isNil()) {
        return nullptr;
    }
    RuntimeScope scope;
    lua_State* state = scope.state();
    lua_glue::StateView lua(state);
    const lua_glue::Object stored = binding::writeLuaValue(lua, value);
    const lua_glue::Object method = binding::writeLuaValue(
        lua, sourceMethod == nullptr ? RuntimeValue() : *sourceMethod);
    stored.push(state);
    auto* owner = static_cast<DescriptorOwner*>(
        luaL_testudata(state, -1, descriptorMetatable));
    DescriptorOwner result = owner == nullptr ? nullptr : *owner;
    if (result != nullptr && sourceMethod != nullptr &&
        !result->metadataFound) {
        lua_getiuservalue(state, -1, 1);
        method.push(state);
        if (!lua_rawequal(state, -1, -2)) {
            result.reset();
        }
        lua_pop(state, 2);
    }
    lua_pop(state, 1);
    return result;
}

std::shared_ptr<EventDescriptor> createDescriptor(
    std::vector<std::string> names) {
    auto result = std::make_shared<EventDescriptor>();
    result->parameters = std::move(names);
    result->accepted.insert(result->parameters.begin(),
                            result->parameters.end());
    return result;
}

DescriptorOwner callableRuntimeParameterDescriptor(const RuntimeValue& method) {
    if (!isFunction(method)) {
        return createDescriptor({});
    }
    RuntimeHandle cache =
        registryTable(BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY, WeakMode::Keys);
    if (DescriptorOwner cached = loadDescriptor(rawGet(cache, method))) {
        return cached;
    }
    DescriptorOwner descriptor =
        createDescriptor(functionParameterNames(method));
    rawSet(cache, method, storeDescriptor(descriptor, RuntimeValue()));
    return descriptor;
}

RuntimeHandle classRuntimeEventCache(const RuntimeValue& classType) {
    RuntimeHandle cache =
        registryTable(BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY, WeakMode::Keys);
    const RuntimeValue cached = rawGet(cache, classType);
    if (isTable(cached)) {
        return intern(cached);
    }
    RuntimeHandle result = table();
    rawSet(cache, classType, result);
    return result;
}

RuntimeValue buildRuntimeEventDescriptor(const RuntimeValue& classType,
                                         const std::string& eventName) {
    for (const RuntimeValue& current : classMro(intern(classType))) {
        const RuntimeValue metadata = typeMetadata(intern(current));
        if (!isTable(metadata)) {
            continue;
        }
        const RuntimeValue event = rawGet(intern(metadata), eventName);
        if (!isTable(event)) {
            continue;
        }
        std::vector<std::string> names;
        const RuntimeValue parameters = rawGet(intern(event), "parameters");
        if (isTable(parameters)) {
            const RuntimeHandle list = intern(parameters);
            const std::size_t count = length(list);
            names.reserve(count);
            for (std::size_t index = 1; index <= count; ++index) {
                const RuntimeValue name = rawGet(list, index);
                if (is<std::string>(name)) {
                    names.push_back(as<std::string>(name));
                }
            }
        }
        auto descriptor = createDescriptor(std::move(names));
        descriptor->metadataFound = true;
        return storeDescriptor(descriptor, RuntimeValue());
    }
    const RuntimeValue method = get(intern(classType), eventName);
    return storeDescriptor(callableRuntimeParameterDescriptor(method), method);
}

}  // namespace

std::shared_ptr<const EventDescriptor> runtimeEventDescriptor(
    const RuntimeValue& method, const RuntimeValue& classType,
    const std::string& eventName) {
    RuntimeHandle members = classRuntimeEventCache(classType);
    RuntimeValue stored = rawGet(members, eventName);
    if (stored.isNil()) {
        stored = buildRuntimeEventDescriptor(classType, eventName);
        rawSet(members, eventName, stored);
    }
    if (auto descriptor = loadDescriptor(stored, &method)) {
        return descriptor;
    }
    return callableRuntimeParameterDescriptor(method);
}

EventArguments eventArguments(const RuntimeValue& keywordArguments) {
    EventArguments result;
    if (isTable(keywordArguments)) {
        for (const auto& [key, value] : entries(intern(keywordArguments))) {
            if (is<std::string>(key)) {
                result.push_back({as<std::string>(key), value});
            }
        }
    }
    return result;
}

void invokeNamedRuntimeMethod(const RuntimeValue& object,
                              const RuntimeValue& method,
                              const RuntimeValue& classType,
                              const std::string& eventName,
                              const EventArguments& values,
                              const RuntimeHandle& keywordSource) {
    if (!isFunction(method)) {
        return;
    }
    const auto descriptor =
        runtimeEventDescriptor(method, classType, eventName);
    if (!keywordSource.isNil()) {
        for (const RuntimeValue& key : keys(keywordSource)) {
            if (is<std::string>(key) &&
                !descriptor->accepted.contains(as<std::string>(key))) {
                throw std::invalid_argument(
                    "Unexpected blueprint event argument '" +
                    as<std::string>(key) + "'");
            }
        }
    } else {
        for (const BlueprintRuntimeFacade::EventArgument& argument : values) {
            if (!descriptor->accepted.contains(argument.name)) {
                throw std::invalid_argument(
                    "Unexpected blueprint event argument '" + argument.name +
                    "'");
            }
        }
    }
    RuntimeValue::Array arguments{object};
    arguments.reserve(descriptor->parameters.size() + 1);
    for (const std::string& name : descriptor->parameters) {
        if (!keywordSource.isNil()) {
            arguments.push_back(get(keywordSource, name));
            continue;
        }
        const auto found = std::find_if(
            values.begin(), values.end(),
            [&name](const BlueprintRuntimeFacade::EventArgument& argument) {
                return argument.name == name;
            });
        arguments.push_back(found == values.end() ? RuntimeValue()
                                                  : found->value);
    }
    static_cast<void>(invoke(intern(method), arguments));
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
