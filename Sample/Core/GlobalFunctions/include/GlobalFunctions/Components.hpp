#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <string>
#include <tuple>
#include <unordered_map>

BIND_FUNCTION_GROUP(name = "Components")

BIND_FUNCTION(name = "_cloneComponentValue", defaults = {nil, nil},
              metadata = false)
RuntimeValue cloneComponentValue(
    const RuntimeValue& value, const RuntimeValue& valueType = RuntimeValue(),
    const RuntimeValue& declaringModule = RuntimeValue());

BIND_FUNCTION(name = "_cloneComponentFieldValue", metadata = false)
RuntimeValue cloneComponentFieldValue(const RuntimeValue& componentType,
                                      const std::string& fieldName,
                                      const RuntimeValue& value);

BIND_FUNCTION(name = "isComponentType", metadata = false)
bool isComponentType(const RuntimeValue& value);

BIND_FUNCTION(name = "getComponentTypes", metadata = false)
RuntimeValue::Map getComponentTypes(const RuntimeValue& classValue);

BIND_FUNCTION(name = "getComponentFieldDefaults", metadata = false)
RuntimeValue::Map getComponentFieldDefaults(const RuntimeValue& componentType);

BIND_FUNCTION(name = "getComponentFieldMap", metadata = false)
std::unordered_map<std::string, std::string> getComponentFieldMap(
    const RuntimeValue& classValue);

BIND_FUNCTION(name = "componentFromData", metadata = false)
RuntimeValue componentFromData(const RuntimeValue& componentType,
                               const RuntimeValue& data);

BIND_FUNCTION(name = "componentToData", metadata = false)
RuntimeValue::Map componentToData(const RuntimeValue& value);

BIND_FUNCTION(name = "_getComponentFieldTarget", multiple_returns = true,
              metadata = false)
std::tuple<RuntimeValue, RuntimeValue, RuntimeValue> getComponentFieldTarget(
    const RuntimeValue& object, const std::string& fieldName);

BIND_FUNCTION(name = "getComponentFieldValue", defaults = {nil},
              metadata = false)
RuntimeValue getComponentFieldValue(
    const RuntimeValue& object, const std::string& fieldName,
    const RuntimeValue& defaultValue = RuntimeValue());

BIND_FUNCTION(name = "setComponentFieldValue", metadata = false)
bool setComponentFieldValue(const RuntimeValue& object,
                            const std::string& fieldName,
                            const RuntimeValue& value);

BIND_FUNCTION(name = "_isBlankValue", metadata = false)
bool isBlankComponentValue(const RuntimeValue& value);

BIND_FUNCTION(name = "_mergeComponentDefaults", metadata = false)
void mergeComponentDefaults(const RuntimeValue& object);

BIND_FUNCTION(name = "normaliseInstanceComponents", metadata = false)
void normaliseInstanceComponents(const RuntimeValue& object);

BIND_FUNCTION(name = "attachInstanceComponents", metadata = false)
RuntimeValue::Array attachInstanceComponents(const RuntimeValue& object);
