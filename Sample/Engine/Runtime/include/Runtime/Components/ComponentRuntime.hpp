#pragma once

#include <RuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <string>
#include <tuple>
#include <unordered_map>

namespace ludork::runtime::components {

LUDORK_RUNTIME_API RuntimeValue cloneComponentValue(
    const RuntimeValue& value, const RuntimeValue& valueType = RuntimeValue(),
    const RuntimeValue& declaringModule = RuntimeValue());

LUDORK_RUNTIME_API RuntimeValue cloneComponentFieldValue(
    const RuntimeValue& componentType, const std::string& fieldName,
    const RuntimeValue& value);

LUDORK_RUNTIME_API bool isComponentType(const RuntimeValue& value);

LUDORK_RUNTIME_API RuntimeValue::Map getComponentTypes(
    const RuntimeValue& classValue);

LUDORK_RUNTIME_API RuntimeValue::Map getComponentFieldDefaults(
    const RuntimeValue& componentType);

LUDORK_RUNTIME_API std::unordered_map<std::string, std::string>
getComponentFieldMap(const RuntimeValue& classValue);

LUDORK_RUNTIME_API RuntimeValue
componentFromData(const RuntimeValue& componentType, const RuntimeValue& data);

LUDORK_RUNTIME_API RuntimeValue::Map componentToData(const RuntimeValue& value);

LUDORK_RUNTIME_API std::tuple<RuntimeValue, RuntimeValue, RuntimeValue>
getComponentFieldTarget(const RuntimeValue& object,
                        const std::string& fieldName);

LUDORK_RUNTIME_API RuntimeValue
getComponentFieldValue(const RuntimeValue& object, const std::string& fieldName,
                       const RuntimeValue& defaultValue = RuntimeValue());

LUDORK_RUNTIME_API bool setComponentFieldValue(const RuntimeValue& object,
                                               const std::string& fieldName,
                                               const RuntimeValue& value);

LUDORK_RUNTIME_API bool isBlankComponentValue(const RuntimeValue& value);

LUDORK_RUNTIME_API void mergeComponentDefaults(const RuntimeValue& object);

LUDORK_RUNTIME_API void normaliseInstanceComponents(const RuntimeValue& object);

LUDORK_RUNTIME_API RuntimeValue::Array attachInstanceComponents(
    const RuntimeValue& object);

}  // namespace ludork::runtime::components
