#include <Runtime/RuntimeReference.hpp>
#include "ClassRuntimeInternal.hpp"

#include <Runtime/Components/ComponentRuntime.hpp>
#include <Runtime/RuntimeProviders.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <RuntimeSession.hpp>
#include <Runtime/TypedDataService.hpp>

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

namespace ludork::runtime::class_runtime_detail {

using namespace ludork::runtime::reference;

RuntimeValue compileGraphTemplate(const RuntimeValue& data,
                                  const RuntimeValue& classType) {
    const RuntimeValue graphData =
        rawGet(ludork::runtime::reference::intern(data), "graph");
    if (!isTable(graphData)) {
        return RuntimeValue();
    }
    return retain(makeValue(runtimeProviders().compileBlueprintGraph(
        identity(graphData), identity(classType))));
}

bool classGraphHasExecutableEvent(const std::string& classPath,
                                  const std::string& eventName) {
    const RuntimeValue rawClass = rawGet(
        requireTable(rawGet(ludork::runtime::reference::intern(resolverState()),
                            "classes")),
        classPath);
    if (isTable(rawClass)) {
        const RuntimeValue rawScriptMixin =
            get(ludork::runtime::reference::intern(rawClass), "scriptMixin");
        if (is<bool>(rawScriptMixin) && as<bool>(rawScriptMixin)) {
            return false;
        }
    }
    const RuntimeValue rawData = rawGet(
        requireTable(rawGet(ludork::runtime::reference::intern(resolverState()),
                            "classData")),
        classPath);
    if (!isTable(rawData)) {
        return false;
    }
    const RuntimeValue rawGraph =
        rawGet(ludork::runtime::reference::intern(rawData), "graph");
    if (!isTable(rawGraph)) {
        return false;
    }
    const RuntimeValue graph = rawGraph;
    const RuntimeValue rawNodeGraph =
        rawGet(ludork::runtime::reference::intern(graph), "nodeGraph");
    const RuntimeValue rawStartNodes =
        rawGet(ludork::runtime::reference::intern(graph), "startNodes");
    if (!isTable(rawNodeGraph) || !isTable(rawStartNodes)) {
        return false;
    }
    const RuntimeValue rawEvent =
        rawGet(ludork::runtime::reference::intern(rawNodeGraph), eventName);
    const RuntimeValue rawStart =
        rawGet(ludork::runtime::reference::intern(rawStartNodes), eventName);
    if (!isTable(rawEvent) ||
        (!is<std::int64_t>(rawStart) && !is<double>(rawStart))) {
        return false;
    }
    const RuntimeValue rawNodes =
        rawGet(ludork::runtime::reference::intern(rawEvent), "nodes");
    const double start = is<std::int64_t>(rawStart)
                             ? static_cast<double>(as<std::int64_t>(rawStart))
                             : as<double>(rawStart);
    return isTable(rawNodes) && start >= 0.0 &&
           static_cast<std::size_t>(start) <
               length(ludork::runtime::reference::intern(rawNodes));
}

RuntimeValue instantiateClassGraph(const std::string& classPath,
                                   const RuntimeValue& parent) {
    RuntimeHandle state = resolverState();
    RuntimeHandle records = requireTable(rawGet(state, "records"));
    RuntimeValue rawRecord = rawGet(records, classPath);
    if (!isTable(rawRecord)) {
        resolveClass(retain(makeValue(classPath)), RuntimeValue());
        rawRecord = rawGet(records, classPath);
    }
    if (!isTable(rawRecord)) {
        return RuntimeValue();
    }
    RuntimeValue record = rawRecord;
    const RuntimeValue rawScriptMixin =
        rawGet(ludork::runtime::reference::intern(record), "scriptMixin");
    if (is<bool>(rawScriptMixin) && as<bool>(rawScriptMixin)) {
        return RuntimeValue();
    }
    RuntimeValue graphTemplate =
        rawGet(ludork::runtime::reference::intern(record), "graphTemplate");
    const RuntimeValue rawGraphCompiled =
        rawGet(ludork::runtime::reference::intern(record), "graphCompiled");
    if (!is<bool>(rawGraphCompiled) || !as<bool>(rawGraphCompiled)) {
        const RuntimeValue rawData =
            rawGet(requireTable(rawGet(state, "classData")), classPath);
        const RuntimeValue classType =
            rawGet(ludork::runtime::reference::intern(record), "class");
        if (isTable(rawData)) {
            graphTemplate = compileGraphTemplate(rawData, classType);
        }
        if (!graphTemplate.isNil()) {
            rawSet(ludork::runtime::reference::intern(record), "graphTemplate",
                   graphTemplate);
        }
        rawSet(ludork::runtime::reference::intern(record), "graphCompiled",
               true);
    }
    if (graphTemplate.isNil()) {
        return RuntimeValue();
    }
    return retain(makeValue(runtimeProviders().instantiateBlueprintGraph(
        identity(graphTemplate), identity(parent))));
}

bool isSequence(const RuntimeValue& value) {
    const RuntimeValue rawCount =
        rawGet(ludork::runtime::reference::intern(value), "n");
    std::size_t count = length(ludork::runtime::reference::intern(value));
    if (!rawCount.isNil()) {
        if (!is<std::int64_t>(rawCount) || as<std::int64_t>(rawCount) < 0) {
            return false;
        }
        count = static_cast<std::size_t>(as<std::int64_t>(rawCount));
    }
    std::size_t entryCount = 0;
    for (const auto& entry :
         entries(ludork::runtime::reference::intern(value))) {
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
    const RuntimeValue typeReference =
        rawGet(ludork::runtime::reference::intern(fieldMetadata), "type");
    const RuntimeValue module =
        rawGet(ludork::runtime::reference::intern(fieldMetadata), "module");
    const RuntimeValue runtimeType = snapshot(typeReference);
    const std::string fieldModule = declaringModule(module);
    const std::string moduleName =
        fieldModule.empty() ? fallbackModule : fieldModule;

    if (is<std::string>(typeReference)) {
        const std::string typeName = as<std::string>(typeReference);
        if (typeName == "bool" || typeName == "int" || typeName == "float" ||
            typeName == "string") {
            return retain(dataValues.resolveTypedDataValue(
                snapshot(value), runtimeType, RuntimeValue::Map{}, moduleName));
        }
        if (typeName == "any" || typeName == "table" || typeName == "list" ||
            typeName == "dict" || typeName == "Pair" ||
            typeName.ends_with("[]")) {
            return deepCopy(value);
        }
    }

    const RuntimeValue component =
        rawGet(ludork::runtime::reference::intern(fieldMetadata), "component");
    if (is<bool>(component) && as<bool>(component)) {
        const RuntimeValue componentType =
            dataValues.resolveMetadataType(runtimeType, moduleName);
        if (!componentType.isNil()) {
            const RuntimeValue resolved =
                ludork::runtime::components::componentFromData(componentType,
                                                               snapshot(value));
            return retain(resolved);
        }
        return deepCopy(value);
    }
    if (isTable(value) && !isSequence(value)) {
        return deepCopy(value);
    }
    if (is<std::string>(value)) {
        return retain(dataValues.evalDataExpression(snapshot(value)));
    }
    return retain(dataValues.resolveTypedDataValue(
        snapshot(value), runtimeType, RuntimeValue::Map{}, moduleName));
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
        targetType = snapshot(rawTargetType);
    } else if (is<std::string>(key)) {
        targetType = dataValues.resolveAttrValueType(
            snapshot(retain(makeValue(parentClass))), as<std::string>(key));
    }
    if (is<std::string>(value)) {
        if (dataValues.shouldEvalValueType(targetType)) {
            return retain(dataValues.evalDataExpression(snapshot(value)));
        }
    }
    const std::string* targetName = targetType.getIf<std::string>();
    if (!targetType.isNil() &&
        (targetName == nullptr || *targetName != "any")) {
        return deepCopy(retain(
            dataValues.resolveTypedDataValue(snapshot(value), targetType)));
    }
    return deepCopy(value);
}

RuntimeValue configReferences(const RuntimeValue& owner) {
    RuntimeHandle cache =
        requireTable(rawGet(ludork::runtime::reference::intern(resolverState()),
                            "configReferences"));
    const RuntimeValue cached = rawGet(cache, owner);
    if (isTable(cached)) {
        return cached;
    }
    RuntimeHandle result = table();
    TypedDataService& dataValues = typedDataService();
    std::vector<RuntimeValue> mro =
        classMro(ludork::runtime::reference::intern(owner));
    for (auto current = mro.rbegin(); current != mro.rend(); ++current) {
        const RuntimeValue currentType = snapshot(retain(makeValue(*current)));
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
    const RuntimeValue rawConfig =
        rawGet(ludork::runtime::reference::intern(reference), 1);
    const RuntimeValue rawSetting =
        rawGet(ludork::runtime::reference::intern(reference), 2);
    if (!is<std::string>(rawConfig) || !is<std::string>(rawSetting)) {
        return value;
    }
    return retain(makeValue(runtimeProviders().config(
        as<std::string>(rawConfig), as<std::string>(rawSetting))));
}

void applyConfigValues(const RuntimeValue& parentClass, RuntimeValue classAttrs,
                       const RuntimeValue& references) {
    for (const auto& entry :
         entries(ludork::runtime::reference::intern(references))) {
        if (!is<std::string>(entry.first) || !isTable(entry.second)) {
            continue;
        }
        const std::string name = as<std::string>(entry.first);
        const RuntimeValue current =
            rawGet(ludork::runtime::reference::intern(classAttrs), name);
        if (!current.isNil()) {
            rawSet(ludork::runtime::reference::intern(classAttrs), name,
                   resolveConfigValue(current, entry.second));
            continue;
        }
        RuntimeValue parentValue =
            get(ludork::runtime::reference::intern(parentClass), name);
        if (parentValue.isNil()) {
            parentValue = retain(makeValue(std::string()));
        }
        const RuntimeValue resolved =
            resolveConfigValue(parentValue, entry.second);
        if (!rawEqual(parentValue, resolved)) {
            rawSet(ludork::runtime::reference::intern(classAttrs), name,
                   resolved);
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
    const RuntimeHandle records = requireTable(
        rawGet(ludork::runtime::reference::intern(resolverState()), "records"));
    RuntimeValue rawRecord = rawGet(records, classPath);
    if (!isTable(rawRecord)) {
        return;
    }
    const RuntimeValue record = rawRecord;
    std::unordered_set<std::string> appliedAttrs;
    while (isTable(rawRecord)) {
        const RuntimeValue current = rawRecord;
        const RuntimeHandle classAttrs = requireTable(
            rawGet(ludork::runtime::reference::intern(current), "attrs"));
        const RuntimeHandle parentClass = requireTable(
            rawGet(ludork::runtime::reference::intern(current), "parent"));
        const RuntimeHandle attrMetadata = requireTable(
            rawGet(ludork::runtime::reference::intern(current), "metadata"));
        const RuntimeHandle attrTypes = requireTable(
            rawGet(ludork::runtime::reference::intern(current), "types"));
        for (const auto& entry :
             entries(ludork::runtime::reference::intern(classAttrs))) {
            if (is<std::string>(entry.first) &&
                appliedAttrs.insert(as<std::string>(entry.first)).second &&
                !hasOwnField(ludork::runtime::reference::intern(self),
                             entry.first)) {
                set(ludork::runtime::reference::intern(self), entry.first,
                    cloneAttrValue(parentClass, entry.first, entry.second,
                                   rawGet(attrMetadata, entry.first),
                                   rawGet(attrTypes, entry.first)));
            }
        }
        rawRecord =
            rawGet(ludork::runtime::reference::intern(current), "parentRecord");
    }
    const RuntimeValue rawInit =
        rawGet(ludork::runtime::reference::intern(record), "parentInit");
    if (isFunction(rawInit)) {
        RuntimeValue::Array values;
        values.reserve(arguments.size() + 1);
        values.push_back(self);
        values.insert(values.end(), arguments.begin(), arguments.end());
        static_cast<void>(
            invoke(ludork::runtime::reference::intern(rawInit), values));
    }
}

}  // namespace ludork::runtime::class_runtime_detail
