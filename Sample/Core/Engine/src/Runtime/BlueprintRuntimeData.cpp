#include "BlueprintRuntimeInternal.hpp"
#include <Runtime/EngineRuntimeServices.hpp>

#include <ClassServices.hpp>
#include <Gameplay/Components/ComponentRuntime.hpp>
#include <LudorkCoreBinding/DynamicValueCodec.hpp>
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

std::string trimRuntimeString(const std::string& value) {
    const std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return std::string();
    }
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

sol::object resolveGeneralDataDictionary(sol::state_view lua,
                                         const sol::object& value) {
    if (value.is<sol::table>()) {
        sol::table result = lua.create_table();
        for (const auto& entry : value.as<sol::table>()) {
            sol::object item = entry.second;
            if (item.is<std::string>()) {
                const std::string text = item.as<std::string>();
                item =
                    trimRuntimeString(text).empty()
                        ? sol::make_object(lua, std::string())
                        : evaluateRuntimeExpression(lua, item, nilObject(lua));
            }
            result.raw_set(entry.first, item);
        }
        return sol::make_object(lua, result);
    }
    if (value.is<std::string>()) {
        const sol::object evaluated =
            evaluateRuntimeExpression(lua, value, nilObject(lua));
        if (evaluated.is<sol::table>()) {
            return resolveGeneralDataDictionary(lua, evaluated);
        }
    }
    return sol::make_object(lua, lua.create_table());
}

bool setBlueprintComponentField(sol::state_view lua, const sol::object& object,
                                const std::string& name,
                                const sol::object& value) {
    static_cast<void>(lua);
    return ludork::engine::components::setComponentFieldValue(
        ludork_core::readLuaValue<RuntimeValue>(object), name,
        ludork_core::readLuaValue<RuntimeValue>(value));
}

void applyBlueprintGeneralData(sol::state_view lua, const sol::object& object,
                               const sol::object& rawData,
                               const sol::object& rawParameterTypes) {
    if (!rawData.is<sol::table>()) {
        return;
    }
    const sol::table parameterTypes = rawParameterTypes.is<sol::table>()
                                          ? rawParameterTypes.as<sol::table>()
                                          : lua.create_table();
    for (const auto& entry : rawData.as<sol::table>()) {
        if (!entry.first.is<std::string>()) {
            continue;
        }
        const std::string name = entry.first.as<std::string>();
        if (name.empty() || name.front() == '_') {
            continue;
        }
        sol::object value = entry.second;
        const sol::object rawParameter = parameterTypes.get<sol::object>(name);
        if (rawParameter.is<sol::table>()) {
            const sol::object rawType =
                rawParameter.as<sol::table>().get<sol::object>("type");
            const std::string type = rawType.is<std::string>()
                                         ? rawType.as<std::string>()
                                         : std::string();
            if (type == "dict") {
                value = resolveGeneralDataDictionary(lua, value);
            } else if (type != "string" && type != "int" && type != "float" &&
                       type != "bool" && type != "list" &&
                       !std::regex_match(type,
                                         std::regex(R"(^tuple\[\d+\]$)"))) {
                value = evaluateRuntimeExpression(lua, value, nilObject(lua));
            }
        } else {
            value = evaluateRuntimeExpression(lua, value, nilObject(lua));
        }
        if (!setBlueprintComponentField(lua, object, name, value)) {
            runtimeAssign(lua, object, sol::make_object(lua, name), value,
                          false);
        }
    }
}

void initializeBlueprintInfo(sol::this_state state, const sol::object& object,
                             const sol::object& dataProvider) {
    sol::state_view lua(state);
    const sol::object rawClass = classType(state, object);
    if (!rawClass.is<sol::table>()) {
        return;
    }
    const sol::object rawInfoType =
        runtimeIndex(lua, rawClass, sol::make_object(lua, "_infoType"), false);
    if (!rawInfoType.is<std::string>() ||
        rawInfoType.as<std::string>().empty()) {
        return;
    }
    const sol::object rawGetGeneralData = runtimeIndex(
        lua, dataProvider, sol::make_object(lua, "getGeneralData"), false);
    if (!rawGetGeneralData.is<sol::protected_function>()) {
        return;
    }
    sol::protected_function getGeneralData =
        rawGetGeneralData.as<sol::protected_function>();
    sol::protected_function_result loaded =
        getGeneralData(rawInfoType.as<std::string>());
    sol::object rawData = checkedResult(lua, loaded);
    if (!rawData.is<sol::table>()) {
        rawData = sol::make_object(lua, lua.create_table());
    }
    const sol::table data = rawData.as<sol::table>();
    const sol::object rawMembers = data.get<sol::object>("members");
    const sol::object rawId =
        runtimeIndex(lua, object, sol::make_object(lua, "ID"), false);
    if (!rawMembers.is<sol::table>() || !rawId.is<std::string>()) {
        return;
    }
    const sol::object member =
        rawMembers.as<sol::table>().get<sol::object>(rawId.as<std::string>());
    if (!member.is<sol::table>()) {
        return;
    }
    sol::object parameters = data.get<sol::object>("params");
    if (!parameters.is<sol::table>()) {
        parameters = sol::make_object(lua, lua.create_table());
    }
    applyBlueprintGeneralData(lua, object, member, parameters);
    const sol::object graphData =
        member.as<sol::table>().get<sol::object>("_graph");
    const sol::object rawGenerator = runtimeIndex(
        lua, dataProvider, sol::make_object(lua, "genGraphFromData"), false);
    if (graphData.get_type() == sol::type::lua_nil ||
        !rawGenerator.is<sol::protected_function>()) {
        return;
    }
    sol::protected_function generator =
        rawGenerator.as<sol::protected_function>();
    sol::protected_function_result generated =
        generator(graphData, object, rawClass);
    const sol::object graph = checkedResult(lua, generated);
    callRuntimeMethodFirst(lua, object, "setInfoGraph", {graph});
}

sol::table registeredBlueprintEvents(sol::state_view lua,
                                     const sol::object& rawClass) {
    sol::object target = rawClass;
    if (!target.is<sol::table>()) {
        target = blueprintEngineType(lua, "InfoBase");
    }
    sol::table result = lua.create_table();
    if (!target.is<sol::table>()) {
        return result;
    }
    sol::table classCache =
        classRuntimeEventCache(lua, target.as<sol::table>());
    const sol::object cached =
        classCache.raw_get<sol::object>("registeredEvents");
    if (cached.is<sol::table>()) {
        return cached.as<sol::table>();
    }
    std::unordered_set<std::string> included;
    std::vector<std::string> events;
    for (const sol::table& current :
         runtimeClassMro(lua, target.as<sol::table>())) {
        const sol::object rawMetadata = runtimeTypeMetadata(lua, current);
        if (!rawMetadata.is<sol::table>()) {
            continue;
        }
        for (const auto& entry : rawMetadata.as<sol::table>()) {
            if (!entry.first.is<std::string>() ||
                !entry.second.is<sol::table>()) {
                continue;
            }
            const sol::object rawType =
                entry.second.as<sol::table>().raw_get<sol::object>("type");
            if (!rawType.is<std::string>() ||
                rawType.as<std::string>() != "event") {
                continue;
            }
            const std::string name = entry.first.as<std::string>();
            if (included.insert(name).second) {
                events.push_back(name);
            }
        }
    }
    std::sort(events.begin(), events.end());
    std::size_t index = 1;
    for (const std::string& event : events) {
        result.raw_set(index++, event);
    }
    classCache.raw_set("registeredEvents", result);
    return result;
}

void clearBlueprintRuntimeCaches(sol::state_view lua) {
    lua.registry().raw_set(BLUEPRINT_IMPLEMENTATION_CACHE_KEY, sol::lua_nil);
    lua.registry().raw_set(BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY, sol::lua_nil);
    lua.registry().raw_set(BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY,
                           sol::lua_nil);
}

}  // namespace ludork::engine::runtime_detail
