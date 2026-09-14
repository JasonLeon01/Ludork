#include <Runtime/RuntimeProviderFacade.hpp>
#include <Runtime/RuntimeReference.hpp>
#include <Runtime/RuntimeReflection.hpp>
#include "ClassRuntimeInternal.hpp"
#include "LuaServices/RuntimeMetadataReferences.hpp"

#include <Runtime/Components/ComponentRuntime.hpp>
#include <Runtime/NodeGraph/Graph.hpp>
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
    return RuntimeValue(runtimeProviders().compileBlueprintGraph(
        identity(graphData), identity(classType)));
}

bool classGraphHasExecutableEvent(const std::string& classPath,
                                  const std::string& eventName) {
    const RuntimeValue record =
        rawGet(requireTable(rawGet(resolverState(), "records")), classPath);
    if (record.isNil()) {
        return false;
    }
    const RuntimeValue graphTemplate = rawGet(intern(record), "graphTemplate");
    if (graphTemplate.isNil()) {
        return false;
    }
    const std::shared_ptr<Graph> graph =
        ludork::Cast<Graph>(ludork::runtime::reference::object(graphTemplate));
    if (graph == nullptr) {
        throw std::runtime_error(
            "Blueprint graph template must be an Engine.Graph");
    }
    return graph->hasExecutableEvent(eventName);
}

RuntimeValue instantiateClassGraph(const std::string& classPath,
                                   const RuntimeValue& parent) {
    RuntimeHandle state = resolverState();
    RuntimeHandle records = requireTable(rawGet(state, "records"));
    RuntimeValue rawRecord = rawGet(records, classPath);
    if (!isTable(rawRecord)) {
        resolveClass(RuntimeValue(classPath), RuntimeValue());
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
    if (graphTemplate.isNil()) {
        return RuntimeValue();
    }
    return RuntimeValue(runtimeProviders().instantiateBlueprintGraph(
        identity(graphTemplate), identity(parent)));
}

std::string declaringModule(const RuntimeValue& value) {
    return is<std::string>(value) ? as<std::string>(value) : std::string();
}

RuntimeValue cloneMetadataValue(const RuntimeValue& value,
                                const RuntimeValue& fieldMetadata,
                                const std::string& fallbackModule,
                                bool stored) {
    TypedDataService& dataValues = typedDataService();
    const RuntimeValue typeReference =
        rawGet(ludork::runtime::reference::intern(fieldMetadata), "type");
    const RuntimeValue module =
        rawGet(ludork::runtime::reference::intern(fieldMetadata), "module");
    const RuntimeValue runtimeType =
        typeReference.getIf<RuntimeHandle>() != nullptr
            ? snapshot(typeReference)
            : typeReference;
    const std::string fieldModule = declaringModule(module);
    const std::string moduleName =
        fieldModule.empty() ? fallbackModule : fieldModule;

    const RuntimeValue component =
        rawGet(ludork::runtime::reference::intern(fieldMetadata), "component");
    if (is<bool>(component) && as<bool>(component)) {
        const RuntimeValue componentType =
            dataValues.resolveMetadataType(runtimeType, moduleName);
        if (!componentType.isNil()) {
            return ludork::runtime::components::componentFromData(componentType,
                                                                  value);
        }
        return deepCopy(value);
    }
    if (!stored) {
        return deepCopy(dataValues.resolveRuntimeTypedValue(value, runtimeType,
                                                            moduleName));
    }
    return deepCopy(dataValues.resolveTypedDataValue(
        value, runtimeType, RuntimeValue::Map{}, moduleName));
}

RuntimeValue cloneAttrValue(const RuntimeValue& parentClass,
                            const RuntimeValue& key, const RuntimeValue& value,
                            const RuntimeValue& rawMetadata,
                            const RuntimeValue& rawTargetType, bool stored) {
    TypedDataService& dataValues = typedDataService();
    if (isTable(rawMetadata)) {
        return cloneMetadataValue(value, rawMetadata, {}, stored);
    }
    RuntimeValue targetType;
    if (!rawTargetType.isNil()) {
        targetType = rawTargetType.getIf<RuntimeHandle>() != nullptr
                         ? snapshot(rawTargetType)
                         : rawTargetType;
    } else if (is<std::string>(key)) {
        targetType =
            dataValues.resolveAttrValueType(parentClass, as<std::string>(key));
    }
    const std::string* targetName = targetType.getIf<std::string>();
    if (!targetType.isNil() &&
        (targetName == nullptr || *targetName != "any")) {
        return deepCopy(
            stored ? dataValues.resolveTypedDataValue(value, targetType)
                   : dataValues.resolveRuntimeTypedValue(value, targetType));
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
    std::vector<RuntimeValue> mro =
        classMro(ludork::runtime::reference::intern(owner));
    for (auto current = mro.rbegin(); current != mro.rend(); ++current) {
        for (const auto& [name, reference] :
             detail::classConfigReferences(*current)) {
            rawSet(result, name, reference);
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
    return RuntimeValue(runtimeProviders().config(as<std::string>(rawConfig),
                                                  as<std::string>(rawSetting)));
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
            parentValue = RuntimeValue(std::string());
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
        const RuntimeHandle copyAttrs = requireTable(
            rawGet(ludork::runtime::reference::intern(current), "copyAttrs"));
        const RuntimeHandle nilAttrs = requireTable(
            rawGet(ludork::runtime::reference::intern(current), "nilAttrs"));
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
                const RuntimeValue value =
                    boolean(rawGet(copyAttrs, entry.first))
                        ? deepCopy(entry.second)
                        : cloneAttrValue(parentClass, entry.first, entry.second,
                                         rawGet(attrMetadata, entry.first),
                                         rawGet(attrTypes, entry.first), false);
                runtimeReflection().setTyped(
                    ludork::runtime::reference::intern(self),
                    as<std::string>(entry.first), value);
            }
        }
        for (const auto& entry : entries(nilAttrs)) {
            if (is<std::string>(entry.first) &&
                appliedAttrs.insert(as<std::string>(entry.first)).second &&
                !hasOwnField(ludork::runtime::reference::intern(self),
                             entry.first)) {
                runtimeReflection().setTyped(
                    ludork::runtime::reference::intern(self),
                    as<std::string>(entry.first), RuntimeValue());
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
