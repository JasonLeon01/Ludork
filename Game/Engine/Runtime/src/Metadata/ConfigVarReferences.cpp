#include "ConfigVarReferences.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace {

std::optional<RuntimeValueView> mapValue(RuntimeValueView value,
                                         const std::string& key) {
    const auto map = value.map();
    return map ? map->find(key) : std::nullopt;
}

void addConfigVarReference(RuntimeValue::Map& result, const std::string& name,
                           RuntimeValueView reference) {
    if (name.empty()) {
        return;
    }
    if (const std::string* text = reference.getIf<std::string>()) {
        if (text->empty()) {
            return;
        }
        const std::size_t separator = text->find('.');
        if (separator == std::string::npos) {
            result[name] = RuntimeValue(
                RuntimeValue::Array{RuntimeValue(*text), RuntimeValue(name)});
            return;
        }
        const std::string configName = text->substr(0, separator);
        const std::string settingName = text->substr(separator + 1);
        if (!configName.empty() && !settingName.empty()) {
            result[name] = RuntimeValue(RuntimeValue::Array{
                RuntimeValue(configName), RuntimeValue(settingName)});
        }
        return;
    }
    std::optional<RuntimeArrayView> values =
        RuntimeValueView(reference).array();
    if (!values || values->size() < 2) {
        return;
    }
    const std::string* configName = (*values)[0].getIf<std::string>();
    const std::string* settingName = (*values)[1].getIf<std::string>();
    if (configName == nullptr || configName->empty() ||
        settingName == nullptr || settingName->empty()) {
        return;
    }
    result[name] = RuntimeValue(RuntimeValue::Array{
        RuntimeValue(*configName), RuntimeValue(*settingName)});
}

void addConfigVarItem(RuntimeValue::Map& result, RuntimeValueView item) {
    if (const std::string* name = item.getIf<std::string>()) {
        if (!name->empty()) {
            result[*name] = RuntimeValue(RuntimeValue::Array{
                RuntimeValue(std::string("System")), RuntimeValue(*name)});
        }
        return;
    }
    std::optional<RuntimeArrayView> values = RuntimeValueView(item).array();
    if (!values || values->size() < 2) {
        return;
    }
    const std::string* name = (*values)[0].getIf<std::string>();
    if (name == nullptr || name->empty()) {
        return;
    }
    if (values->size() >= 3) {
        const RuntimeValue reference(RuntimeValue::Array{
            (*values)[1].toValue(), (*values)[2].toValue()});
        addConfigVarReference(result, *name, reference);
        return;
    }
    addConfigVarReference(result, *name, (*values)[1]);
}
}  // namespace

namespace ludork::runtime::detail {

RuntimeValue::Map parseConfigVarReferences(const RuntimeValue& metadata) {
    RuntimeValue::Map result;
    const auto rawVars = mapValue(metadata, "ConfigVars");
    if (!rawVars) {
        return result;
    }
    if (std::optional<RuntimeMapView> values =
            RuntimeValueView(*rawVars).map()) {
        for (const auto& [name, reference] : *values) {
            addConfigVarReference(result, name, reference);
        }
        return result;
    }
    if (std::optional<RuntimeArrayView> values =
            RuntimeValueView(*rawVars).array()) {
        for (RuntimeValueView item : *values) {
            addConfigVarItem(result, item);
        }
    }
    return result;
}

}  // namespace ludork::runtime::detail
