#include "BlueprintRuntimeInternal.hpp"
#include <Runtime/EngineRuntimeServices.hpp>

#include <ClassServices.hpp>
#include <Gameplay/Components/ComponentRuntime.hpp>
#include <LudorkCoreBinding/DynamicValueCodec.hpp>
#include <NodeGraph/Graph.hpp>
#include <Runtime/EngineClassRuntime.hpp>
#include <Utf8Path.hpp>

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
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::engine::runtime_detail {

bool setBlueprintComponentField(sol::state_view lua, const sol::object& object,
                                const std::string& name,
                                const sol::object& value) {
    static_cast<void>(lua);
    return ludork::engine::components::setComponentFieldValue(
        ludork_core::readLuaValue<RuntimeValue>(object), name,
        ludork_core::readLuaValue<RuntimeValue>(value));
}

void applyBlueprintGeneralData(sol::state_view lua, const sol::object& object,
                               const sol::object& rawData) {
    if (!rawData.is<sol::table>()) {
        throw std::invalid_argument("General Data member must be a table");
    }
    for (const auto& entry : rawData.as<sol::table>()) {
        if (!entry.first.is<std::string>()) {
            throw std::invalid_argument(
                "General Data field name must be a string");
        }
        const std::string name = entry.first.as<std::string>();
        if (name.empty() || name.front() == '_') {
            continue;
        }
        if (!setBlueprintComponentField(lua, object, name, entry.second)) {
            runtimeAssign(
                lua, object, sol::make_object(lua, name),
                ludork::standard::class_runtime::deepCopy(lua, entry.second),
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
        throw std::runtime_error(
            "General Data provider must expose getGeneralData");
    }
    sol::protected_function getGeneralData =
        rawGetGeneralData.as<sol::protected_function>();
    sol::protected_function_result loaded =
        getGeneralData(rawInfoType.as<std::string>());
    sol::object rawData = checkedResult(lua, loaded);
    if (!rawData.is<sol::table>()) {
        throw std::runtime_error("General Data definition must be a table");
    }
    const sol::table data = rawData.as<sol::table>();
    const sol::object rawMembers = data.get<sol::object>("members");
    const sol::object rawId =
        runtimeIndex(lua, object, sol::make_object(lua, "ID"), false);
    if (!rawMembers.is<sol::table>() || !rawId.is<std::string>()) {
        throw std::runtime_error(
            "General Data definition requires members and a string Info ID");
    }
    const sol::object member =
        rawMembers.as<sol::table>().get<sol::object>(rawId.as<std::string>());
    if (!member.is<sol::table>()) {
        throw std::runtime_error("General Data member not found: " +
                                 rawId.as<std::string>());
    }
    applyBlueprintGeneralData(lua, object, member);
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
