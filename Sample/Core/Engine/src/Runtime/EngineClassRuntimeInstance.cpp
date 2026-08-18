#include "EngineClassRuntimeInternal.hpp"
#include "RuntimeSubsystemServices.hpp"

#include <ClassServices.hpp>
#include <Gameplay/Components/ComponentRuntime.hpp>
#include <LudorkCoreBinding.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <RuntimeSession.hpp>
#include <Utils/DataValue.hpp>

#include <sol2/sol.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::engine::class_runtime_detail {

sol::object compileGraphTemplate(sol::state_view lua, const sol::table& data,
                                 const sol::object& classType) {
    const sol::object graphData = data.raw_get<sol::object>("graph");
    if (!graphData.is<sol::table>()) {
        return nilObject(lua);
    }
    return callRuntimeServiceFirst(lua, "blueprint.compileGraph",
                                   {graphData, classType});
}

bool classGraphHasExecutableEvent(sol::state_view lua,
                                  const std::string& classPath,
                                  const std::string& eventName) {
    const sol::object rawClass =
        resolverState(lua).raw_get<sol::table>("classes").raw_get<sol::object>(
            classPath);
    if (rawClass.is<sol::table>()) {
        const sol::object rawScriptMixin =
            rawClass.as<sol::table>().get<sol::object>("scriptMixin");
        if (rawScriptMixin.is<bool>() && rawScriptMixin.as<bool>()) {
            return false;
        }
    }
    const sol::object rawData = resolverState(lua)
                                    .raw_get<sol::table>("classData")
                                    .raw_get<sol::object>(classPath);
    if (!rawData.is<sol::table>()) {
        return false;
    }
    const sol::object rawGraph =
        rawData.as<sol::table>().raw_get<sol::object>("graph");
    if (!rawGraph.is<sol::table>()) {
        return false;
    }
    const sol::table graph = rawGraph.as<sol::table>();
    const sol::object rawNodeGraph = graph.raw_get<sol::object>("nodeGraph");
    const sol::object rawStartNodes = graph.raw_get<sol::object>("startNodes");
    if (!rawNodeGraph.is<sol::table>() || !rawStartNodes.is<sol::table>()) {
        return false;
    }
    const sol::object rawEvent =
        rawNodeGraph.as<sol::table>().raw_get<sol::object>(eventName);
    const sol::object rawStart =
        rawStartNodes.as<sol::table>().raw_get<sol::object>(eventName);
    if (!rawEvent.is<sol::table>() ||
        (!rawStart.is<std::int64_t>() && !rawStart.is<double>())) {
        return false;
    }
    const sol::object rawNodes =
        rawEvent.as<sol::table>().raw_get<sol::object>("nodes");
    const double start = rawStart.is<std::int64_t>()
                             ? static_cast<double>(rawStart.as<std::int64_t>())
                             : rawStart.as<double>();
    return rawNodes.is<sol::table>() && start >= 0.0 &&
           static_cast<std::size_t>(start) < rawNodes.as<sol::table>().size();
}

sol::object instantiateClassGraph(sol::state_view lua,
                                  const std::string& classPath,
                                  const sol::object& parent) {
    sol::table state = resolverState(lua);
    sol::table records = state.raw_get<sol::table>("records");
    sol::object rawRecord = records.raw_get<sol::object>(classPath);
    if (!rawRecord.is<sol::table>()) {
        resolveClass(lua, sol::make_object(lua, classPath), nilObject(lua));
        rawRecord = records.raw_get<sol::object>(classPath);
    }
    if (!rawRecord.is<sol::table>()) {
        return nilObject(lua);
    }
    sol::table record = rawRecord.as<sol::table>();
    const sol::object rawScriptMixin =
        record.raw_get<sol::object>("scriptMixin");
    if (rawScriptMixin.is<bool>() && rawScriptMixin.as<bool>()) {
        return nilObject(lua);
    }
    sol::object graphTemplate = record.raw_get<sol::object>("graphTemplate");
    const sol::object rawGraphCompiled =
        record.raw_get<sol::object>("graphCompiled");
    if (!rawGraphCompiled.is<bool>() || !rawGraphCompiled.as<bool>()) {
        const sol::object rawData = state.raw_get<sol::table>("classData")
                                        .raw_get<sol::object>(classPath);
        const sol::object classType = record.raw_get<sol::object>("class");
        if (rawData.is<sol::table>()) {
            graphTemplate =
                compileGraphTemplate(lua, rawData.as<sol::table>(), classType);
        }
        if (graphTemplate.valid() &&
            graphTemplate.get_type() != sol::type::lua_nil) {
            record.raw_set("graphTemplate", graphTemplate);
        }
        record.raw_set("graphCompiled", true);
    }
    if (!graphTemplate.valid() ||
        graphTemplate.get_type() == sol::type::lua_nil) {
        return nilObject(lua);
    }
    return callRuntimeServiceFirst(lua, "blueprint.instantiateGraphTemplate",
                                   {graphTemplate, parent});
}

std::vector<sol::table> classMro(const sol::table& classTable) {
    std::vector<sol::table> result;
    sol::state_view lua(classTable.lua_state());
    const sol::table mro = ludork::standard::class_runtime::getMroCopy(
        lua, sol::make_object(lua, classTable));
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object current = mro.raw_get<sol::object>(index);
        if (current.is<sol::table>()) {
            result.push_back(current.as<sol::table>());
        }
    }
    if (result.empty()) {
        result.push_back(classTable);
    }
    return result;
}

bool isSequence(const sol::table& value) {
    const sol::object rawCount = value.raw_get<sol::object>("n");
    std::size_t count = value.size();
    if (rawCount.valid() && rawCount.get_type() != sol::type::lua_nil) {
        if (!rawCount.is<lua_Integer>() || rawCount.as<lua_Integer>() < 0) {
            return false;
        }
        count = static_cast<std::size_t>(rawCount.as<lua_Integer>());
    }
    std::size_t entries = 0;
    for (const auto& entry : value) {
        if (entry.first.is<std::string>() &&
            entry.first.as<std::string>() == "n") {
            continue;
        }
        if (!entry.first.is<lua_Integer>()) {
            return false;
        }
        const lua_Integer index = entry.first.as<lua_Integer>();
        if (index < 1 || static_cast<std::size_t>(index) > count) {
            return false;
        }
        ++entries;
    }
    return entries == count;
}

RuntimeValue runtimeValue(const sol::object& value) {
    return ludork_core::readLuaValue<RuntimeValue>(value);
}

sol::object luaValue(sol::state_view lua, const RuntimeValue& value) {
    return ludork_core::writeLuaValue(lua, value);
}

std::string declaringModule(const sol::object& value) {
    return value.is<std::string>() ? value.as<std::string>() : std::string();
}

sol::object cloneMetadataValue(sol::state_view lua, const sol::object& value,
                               const sol::table& fieldMetadata,
                               const std::string& fallbackModule) {
    DataValueService& dataValues = dataValueService();
    const sol::object typeReference =
        fieldMetadata.raw_get<sol::object>("type");
    const sol::object module = fieldMetadata.raw_get<sol::object>("module");
    const RuntimeValue runtimeType = runtimeValue(typeReference);
    const std::string fieldModule = declaringModule(module);
    const std::string moduleName =
        fieldModule.empty() ? fallbackModule : fieldModule;

    if (typeReference.is<std::string>()) {
        const std::string typeName = typeReference.as<std::string>();
        if (typeName == "bool" || typeName == "int" || typeName == "float" ||
            typeName == "string") {
            return luaValue(lua, dataValues.resolveTypedDataValue(
                                     runtimeValue(value), runtimeType,
                                     RuntimeValue::Map{}, moduleName));
        }
        if (typeName == "any" || typeName == "table" || typeName == "list" ||
            typeName == "dict" || typeName == "Pair" ||
            typeName.ends_with("[]")) {
            return ludork::standard::class_runtime::deepCopy(lua, value);
        }
    }

    const sol::object component =
        fieldMetadata.raw_get<sol::object>("component");
    if (component.is<bool>() && component.as<bool>()) {
        const RuntimeValue componentType =
            dataValues.resolveMetadataType(runtimeType, moduleName);
        if (!componentType.isNil()) {
            const RuntimeValue resolved =
                ludork::engine::components::componentFromData(
                    componentType, runtimeValue(value));
            return luaValue(lua, resolved);
        }
        return ludork::standard::class_runtime::deepCopy(lua, value);
    }
    if (value.is<sol::table>() && !isSequence(value.as<sol::table>())) {
        return ludork::standard::class_runtime::deepCopy(lua, value);
    }
    if (value.is<std::string>()) {
        return luaValue(lua,
                        dataValues.evalDataExpression(runtimeValue(value)));
    }
    return luaValue(
        lua, dataValues.resolveTypedDataValue(runtimeValue(value), runtimeType,
                                              RuntimeValue::Map{}, moduleName));
}

sol::object cloneAttrValue(sol::state_view lua, const sol::table& parentClass,
                           const sol::object& key, const sol::object& value,
                           const sol::object& rawMetadata,
                           const sol::object& rawTargetType) {
    DataValueService& dataValues = dataValueService();
    if (rawMetadata.is<sol::table>()) {
        return cloneMetadataValue(lua, value, rawMetadata.as<sol::table>());
    }
    RuntimeValue targetType;
    if (rawTargetType.valid() &&
        rawTargetType.get_type() != sol::type::lua_nil) {
        targetType = runtimeValue(rawTargetType);
    } else if (key.is<std::string>()) {
        targetType = dataValues.resolveAttrValueType(
            runtimeValue(sol::make_object(lua, parentClass)),
            key.as<std::string>());
    }
    if (value.is<std::string>()) {
        if (dataValues.shouldEvalValueType(targetType)) {
            return luaValue(lua,
                            dataValues.evalDataExpression(runtimeValue(value)));
        }
    }
    const std::string* targetName = targetType.getIf<std::string>();
    if (!targetType.isNil() &&
        (targetName == nullptr || *targetName != "any")) {
        return ludork::standard::class_runtime::deepCopy(
            lua, luaValue(lua, dataValues.resolveTypedDataValue(
                                   runtimeValue(value), targetType)));
    }
    return ludork::standard::class_runtime::deepCopy(lua, value);
}

sol::table configReferences(sol::state_view lua, const sol::table& owner) {
    sol::table cache =
        resolverState(lua).raw_get<sol::table>("configReferences");
    const sol::object cached = cache.raw_get<sol::object>(owner);
    if (cached.is<sol::table>()) {
        return cached.as<sol::table>();
    }
    sol::table result = lua.create_table();
    DataValueService& dataValues = dataValueService();
    std::vector<sol::table> mro = classMro(owner);
    for (auto current = mro.rbegin(); current != mro.rend(); ++current) {
        const RuntimeValue currentType =
            runtimeValue(sol::make_object(lua, *current));
        const RuntimeValue metadata =
            dataValues.getClassTypeMetadata(currentType).first;
        const RuntimeValue::Map* metadataFields =
            metadata.getIf<RuntimeValue::Map>();
        if (metadataFields == nullptr) {
            continue;
        }
        const auto metaIterator = metadataFields->find("Meta");
        if (metaIterator == metadataFields->end()) {
            continue;
        }
        for (const auto& [name, reference] :
             getConfigVars(metaIterator->second)) {
            result.raw_set(name, luaValue(lua, reference));
        }
    }
    cache.raw_set(owner, result);
    return result;
}

sol::object resolveConfigValue(sol::state_view lua, const sol::object& value,
                               const sol::table& reference) {
    if (!value.is<std::string>() || !value.as<std::string>().empty()) {
        return value;
    }
    const sol::object rawConfig = reference.raw_get<sol::object>(1);
    const sol::object rawSetting = reference.raw_get<sol::object>(2);
    if (!rawConfig.is<std::string>() || !rawSetting.is<std::string>()) {
        return value;
    }
    const std::vector<RuntimeValue> resolved = resolveRuntime(
        "config.resolve", {RuntimeValue(rawConfig.as<std::string>()),
                           RuntimeValue(rawSetting.as<std::string>())});
    if (resolved.empty()) {
        return value;
    }
    const std::string* resolvedValue = resolved.front().getIf<std::string>();
    return resolvedValue == nullptr ? value
                                    : sol::make_object(lua, *resolvedValue);
}

void applyConfigValues(sol::state_view lua, const sol::table& parentClass,
                       sol::table classAttrs, const sol::table& references) {
    for (const auto& entry : references) {
        if (!entry.first.is<std::string>() || !entry.second.is<sol::table>()) {
            continue;
        }
        const std::string name = entry.first.as<std::string>();
        const sol::object current = classAttrs.raw_get<sol::object>(name);
        if (current.valid() && current.get_type() != sol::type::lua_nil) {
            classAttrs.raw_set(
                name, resolveConfigValue(lua, current,
                                         entry.second.as<sol::table>()));
            continue;
        }
        sol::object parentValue = parentClass.get<sol::object>(name);
        if (!parentValue.valid() ||
            parentValue.get_type() == sol::type::lua_nil) {
            parentValue = sol::make_object(lua, std::string());
        }
        const sol::object resolved =
            resolveConfigValue(lua, parentValue, entry.second.as<sol::table>());
        if (!ludork::standard::class_runtime::rawEqual(parentValue, resolved)) {
            classAttrs.raw_set(name, resolved);
        }
    }
}

void initializeGeneratedInstance(lua_State* state, const std::string& classPath,
                                 const sol::object& self,
                                 const sol::variadic_args& arguments) {
    ludork::standard::LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    sol::state_view lua(state);
    const sol::table records =
        resolverState(lua).raw_get<sol::table>("records");
    sol::object rawRecord = records.raw_get<sol::object>(classPath);
    if (!rawRecord.is<sol::table>()) {
        return;
    }
    const sol::table record = rawRecord.as<sol::table>();
    std::unordered_set<std::string> appliedAttrs;
    while (rawRecord.is<sol::table>()) {
        const sol::table current = rawRecord.as<sol::table>();
        const sol::table classAttrs = current.raw_get<sol::table>("attrs");
        const sol::table parentClass = current.raw_get<sol::table>("parent");
        const sol::table attrMetadata = current.raw_get<sol::table>("metadata");
        const sol::table attrTypes = current.raw_get<sol::table>("types");
        for (const auto& entry : classAttrs) {
            if (entry.first.is<std::string>() &&
                appliedAttrs.insert(entry.first.as<std::string>()).second &&
                !ludork::standard::class_runtime::hasOwnField(lua, self,
                                                              entry.first)) {
                ludork::standard::class_runtime::protectedSet(
                    lua, self, entry.first,
                    cloneAttrValue(
                        lua, parentClass, entry.first, entry.second,
                        attrMetadata.raw_get<sol::object>(entry.first),
                        attrTypes.raw_get<sol::object>(entry.first)));
            }
        }
        rawRecord = current.raw_get<sol::object>("parentRecord");
    }
    const sol::object rawInit = record.raw_get<sol::object>("parentInit");
    if (rawInit.is<sol::protected_function>()) {
        std::vector<sol::object> values;
        values.reserve(arguments.size() + 1);
        values.push_back(self);
        for (const auto& argument : arguments) {
            values.push_back(argument.get<sol::object>());
        }
        lua_State* state = lua.lua_state();
        const int stackBase = lua_gettop(state);
        static_cast<void>(ludork::engine::runtime_detail::invokeRuntimeFunction(
            state, rawInit, values, "runtime class init arguments"));
        lua_settop(state, stackBase);
    }
    ludork::engine::components::normaliseInstanceComponents(
        ludork_core::readLuaValue<RuntimeValue>(self));
}

}  // namespace ludork::engine::class_runtime_detail
