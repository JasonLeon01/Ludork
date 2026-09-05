#include <Runtime/RuntimeReference.hpp>
#include "EngineClassRuntimeInternal.hpp"

#include <Gameplay/Components/ComponentRuntime.hpp>
#include <Runtime/RuntimeProviders.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <RuntimeSession.hpp>
#include <Utils/DataValue.hpp>

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

using namespace ludork::runtime::reference;

RuntimeValue compileGraphTemplate(const RuntimeValue& data,
                                  const RuntimeValue& classType) {
    const RuntimeValue graphData = rawGet(data, "graph");
    if (!isTable(graphData)) {
        return RuntimeValue();
    }
    return retain(makeValue(runtimeProviders().compileBlueprintGraph(
        identity(graphData), identity(classType))));
}

bool classGraphHasExecutableEvent(const std::string& classPath,
                                  const std::string& eventName) {
    const RuntimeValue rawClass =
        rawGet(requireTable(rawGet(resolverState(), "classes")), classPath);
    if (isTable(rawClass)) {
        const RuntimeValue rawScriptMixin = get(rawClass, "scriptMixin");
        if (is<bool>(rawScriptMixin) && as<bool>(rawScriptMixin)) {
            return false;
        }
    }
    const RuntimeValue rawData =
        rawGet(requireTable(rawGet(resolverState(), "classData")), classPath);
    if (!isTable(rawData)) {
        return false;
    }
    const RuntimeValue rawGraph = rawGet(rawData, "graph");
    if (!isTable(rawGraph)) {
        return false;
    }
    const RuntimeValue graph = rawGraph;
    const RuntimeValue rawNodeGraph = rawGet(graph, "nodeGraph");
    const RuntimeValue rawStartNodes = rawGet(graph, "startNodes");
    if (!isTable(rawNodeGraph) || !isTable(rawStartNodes)) {
        return false;
    }
    const RuntimeValue rawEvent = rawGet(rawNodeGraph, eventName);
    const RuntimeValue rawStart = rawGet(rawStartNodes, eventName);
    if (!isTable(rawEvent) ||
        (!is<std::int64_t>(rawStart) && !is<double>(rawStart))) {
        return false;
    }
    const RuntimeValue rawNodes = rawGet(rawEvent, "nodes");
    const double start = is<std::int64_t>(rawStart)
                             ? static_cast<double>(as<std::int64_t>(rawStart))
                             : as<double>(rawStart);
    return isTable(rawNodes) && start >= 0.0 &&
           static_cast<std::size_t>(start) < length(rawNodes);
}

RuntimeValue instantiateClassGraph(const std::string& classPath,
                                   const RuntimeValue& parent) {
    RuntimeValue state = resolverState();
    RuntimeValue records = requireTable(rawGet(state, "records"));
    RuntimeValue rawRecord = rawGet(records, classPath);
    if (!isTable(rawRecord)) {
        resolveClass(retain(makeValue(classPath)), RuntimeValue());
        rawRecord = rawGet(records, classPath);
    }
    if (!isTable(rawRecord)) {
        return RuntimeValue();
    }
    RuntimeValue record = rawRecord;
    const RuntimeValue rawScriptMixin = rawGet(record, "scriptMixin");
    if (is<bool>(rawScriptMixin) && as<bool>(rawScriptMixin)) {
        return RuntimeValue();
    }
    RuntimeValue graphTemplate = rawGet(record, "graphTemplate");
    const RuntimeValue rawGraphCompiled = rawGet(record, "graphCompiled");
    if (!is<bool>(rawGraphCompiled) || !as<bool>(rawGraphCompiled)) {
        const RuntimeValue rawData =
            rawGet(requireTable(rawGet(state, "classData")), classPath);
        const RuntimeValue classType = rawGet(record, "class");
        if (isTable(rawData)) {
            graphTemplate = compileGraphTemplate(rawData, classType);
        }
        if (!graphTemplate.isNil()) {
            rawSet(record, "graphTemplate", graphTemplate);
        }
        rawSet(record, "graphCompiled", true);
    }
    if (graphTemplate.isNil()) {
        return RuntimeValue();
    }
    return retain(makeValue(runtimeProviders().instantiateBlueprintGraph(
        identity(graphTemplate), identity(parent))));
}

bool isSequence(const RuntimeValue& value) {
    const RuntimeValue rawCount = rawGet(value, "n");
    std::size_t count = length(value);
    if (!rawCount.isNil()) {
        if (!is<std::int64_t>(rawCount) || as<std::int64_t>(rawCount) < 0) {
            return false;
        }
        count = static_cast<std::size_t>(as<std::int64_t>(rawCount));
    }
    std::size_t entryCount = 0;
    for (const auto& entry : entries(value)) {
        if (is<std::string>(entry.first) &&
            as<std::string>(entry.first) == "n") {
            continue;
        }
        if (!is<std::int64_t>(entry.first)) {
            return false;
        }
        const std::int64_t index = as<std::int64_t>(entry.first);
        if (index < 1 || static_cast<std::size_t>(index) > count) {
            return false;
        }
        ++entryCount;
    }
    return entryCount == count;
}

std::string declaringModule(const RuntimeValue& value) {
    return is<std::string>(value) ? as<std::string>(value) : std::string();
}

RuntimeValue cloneMetadataValue(const RuntimeValue& value,
                                const RuntimeValue& fieldMetadata,
                                const std::string& fallbackModule) {
    TypedDataService& dataValues = typedDataService();
    const RuntimeValue typeReference = rawGet(fieldMetadata, "type");
    const RuntimeValue module = rawGet(fieldMetadata, "module");
    const RuntimeValue runtimeType = data(typeReference);
    const std::string fieldModule = declaringModule(module);
    const std::string moduleName =
        fieldModule.empty() ? fallbackModule : fieldModule;

    if (is<std::string>(typeReference)) {
        const std::string typeName = as<std::string>(typeReference);
        if (typeName == "bool" || typeName == "int" || typeName == "float" ||
            typeName == "string") {
            return retain(dataValues.resolveTypedDataValue(
                data(value), runtimeType, RuntimeValue::Map{}, moduleName));
        }
        if (typeName == "any" || typeName == "table" || typeName == "list" ||
            typeName == "dict" || typeName == "Pair" ||
            typeName.ends_with("[]")) {
            return deepCopy(value);
        }
    }

    const RuntimeValue component = rawGet(fieldMetadata, "component");
    if (is<bool>(component) && as<bool>(component)) {
        const RuntimeValue componentType =
            dataValues.resolveMetadataType(runtimeType, moduleName);
        if (!componentType.isNil()) {
            const RuntimeValue resolved =
                ludork::engine::components::componentFromData(componentType,
                                                              data(value));
            return retain(resolved);
        }
        return deepCopy(value);
    }
    if (isTable(value) && !isSequence(value)) {
        return deepCopy(value);
    }
    if (is<std::string>(value)) {
        return retain(dataValues.evalDataExpression(data(value)));
    }
    return retain(dataValues.resolveTypedDataValue(
        data(value), runtimeType, RuntimeValue::Map{}, moduleName));
}

RuntimeValue cloneAttrValue(const RuntimeValue& parentClass,
                            const RuntimeValue& key, const RuntimeValue& value,
                            const RuntimeValue& rawMetadata,
                            const RuntimeValue& rawTargetType) {
    TypedDataService& dataValues = typedDataService();
    if (isTable(rawMetadata)) {
        return cloneMetadataValue(value, rawMetadata);
    }
    RuntimeValue targetType;
    if (!rawTargetType.isNil()) {
        targetType = data(rawTargetType);
    } else if (is<std::string>(key)) {
        targetType = dataValues.resolveAttrValueType(
            data(retain(makeValue(parentClass))), as<std::string>(key));
    }
    if (is<std::string>(value)) {
        if (dataValues.shouldEvalValueType(targetType)) {
            return retain(dataValues.evalDataExpression(data(value)));
        }
    }
    const std::string* targetName = targetType.getIf<std::string>();
    if (!targetType.isNil() &&
        (targetName == nullptr || *targetName != "any")) {
        return deepCopy(
            retain(dataValues.resolveTypedDataValue(data(value), targetType)));
    }
    return deepCopy(value);
}

RuntimeValue configReferences(const RuntimeValue& owner) {
    RuntimeValue cache =
        requireTable(rawGet(resolverState(), "configReferences"));
    const RuntimeValue cached = rawGet(cache, owner);
    if (isTable(cached)) {
        return cached;
    }
    RuntimeValue result = table();
    TypedDataService& dataValues = typedDataService();
    std::vector<RuntimeValue> mro = classMro(owner);
    for (auto current = mro.rbegin(); current != mro.rend(); ++current) {
        const RuntimeValue currentType = data(retain(makeValue(*current)));
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
            rawSet(result, name, retain(reference));
        }
    }
    rawSet(cache, owner, result);
    return result;
}

RuntimeValue resolveConfigValue(const RuntimeValue& value,
                                const RuntimeValue& reference) {
    if (!is<std::string>(value) || !as<std::string>(value).empty()) {
        return value;
    }
    const RuntimeValue rawConfig = rawGet(reference, 1);
    const RuntimeValue rawSetting = rawGet(reference, 2);
    if (!is<std::string>(rawConfig) || !is<std::string>(rawSetting)) {
        return value;
    }
    return retain(makeValue(runtimeProviders().config(
        as<std::string>(rawConfig), as<std::string>(rawSetting))));
}

void applyConfigValues(const RuntimeValue& parentClass, RuntimeValue classAttrs,
                       const RuntimeValue& references) {
    for (const auto& entry : entries(references)) {
        if (!is<std::string>(entry.first) || !isTable(entry.second)) {
            continue;
        }
        const std::string name = as<std::string>(entry.first);
        const RuntimeValue current = rawGet(classAttrs, name);
        if (!current.isNil()) {
            rawSet(classAttrs, name, resolveConfigValue(current, entry.second));
            continue;
        }
        RuntimeValue parentValue = get(parentClass, name);
        if (parentValue.isNil()) {
            parentValue = retain(makeValue(std::string()));
        }
        const RuntimeValue resolved =
            resolveConfigValue(parentValue, entry.second);
        if (!rawEqual(parentValue, resolved)) {
            rawSet(classAttrs, name, resolved);
        }
    }
}

void initializeGeneratedInstance(lua_State* state, const std::string& classPath,
                                 const RuntimeValue& self,
                                 const RuntimeValue::Array& arguments) {
    ludork::standard::LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    const RuntimeValue records =
        requireTable(rawGet(resolverState(), "records"));
    RuntimeValue rawRecord = rawGet(records, classPath);
    if (!isTable(rawRecord)) {
        return;
    }
    const RuntimeValue record = rawRecord;
    std::unordered_set<std::string> appliedAttrs;
    while (isTable(rawRecord)) {
        const RuntimeValue current = rawRecord;
        const RuntimeValue classAttrs = requireTable(rawGet(current, "attrs"));
        const RuntimeValue parentClass =
            requireTable(rawGet(current, "parent"));
        const RuntimeValue attrMetadata =
            requireTable(rawGet(current, "metadata"));
        const RuntimeValue attrTypes = requireTable(rawGet(current, "types"));
        for (const auto& entry : entries(classAttrs)) {
            if (is<std::string>(entry.first) &&
                appliedAttrs.insert(as<std::string>(entry.first)).second &&
                !hasOwnField(self, entry.first)) {
                set(self, entry.first,
                    cloneAttrValue(parentClass, entry.first, entry.second,
                                   rawGet(attrMetadata, entry.first),
                                   rawGet(attrTypes, entry.first)));
            }
        }
        rawRecord = rawGet(current, "parentRecord");
    }
    const RuntimeValue rawInit = rawGet(record, "parentInit");
    if (isFunction(rawInit)) {
        RuntimeValue::Array values;
        values.reserve(arguments.size() + 1);
        values.push_back(self);
        values.insert(values.end(), arguments.begin(), arguments.end());
        static_cast<void>(invoke(rawInit, values));
    }
}

}  // namespace ludork::engine::class_runtime_detail
