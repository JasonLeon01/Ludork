#pragma once

#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <string>
#include <tuple>
#include <unordered_map>

namespace ludork::engine::components {

LUDORK_ENGINE_API RuntimeValue cloneComponentValue(
    const RuntimeValue& value, const RuntimeValue& valueType = RuntimeValue(),
    const RuntimeValue& declaringModule = RuntimeValue());

LUDORK_ENGINE_API RuntimeValue cloneComponentFieldValue(
    const RuntimeValue& componentType, const std::string& fieldName,
    const RuntimeValue& value);

LUDORK_ENGINE_API bool isComponentType(const RuntimeValue& value);

LUDORK_ENGINE_API RuntimeValue::Map getComponentTypes(
    const RuntimeValue& classValue);

LUDORK_ENGINE_API RuntimeValue::Map getComponentFieldDefaults(
    const RuntimeValue& componentType);

LUDORK_ENGINE_API std::unordered_map<std::string, std::string>
getComponentFieldMap(const RuntimeValue& classValue);

LUDORK_ENGINE_API RuntimeValue
componentFromData(const RuntimeValue& componentType, const RuntimeValue& data);

LUDORK_ENGINE_API RuntimeValue::Map componentToData(const RuntimeValue& value);

LUDORK_ENGINE_API std::tuple<RuntimeValue, RuntimeValue, RuntimeValue>
getComponentFieldTarget(const RuntimeValue& object,
                        const std::string& fieldName);

LUDORK_ENGINE_API RuntimeValue
getComponentFieldValue(const RuntimeValue& object, const std::string& fieldName,
                       const RuntimeValue& defaultValue = RuntimeValue());

LUDORK_ENGINE_API bool setComponentFieldValue(const RuntimeValue& object,
                                              const std::string& fieldName,
                                              const RuntimeValue& value);

LUDORK_ENGINE_API bool isBlankComponentValue(const RuntimeValue& value);

LUDORK_ENGINE_API void mergeComponentDefaults(const RuntimeValue& object);

LUDORK_ENGINE_API void normaliseInstanceComponents(const RuntimeValue& object);

LUDORK_ENGINE_API RuntimeValue::Array attachInstanceComponents(
    const RuntimeValue& object);

}  // namespace ludork::engine::components
