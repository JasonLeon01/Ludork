#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class LUDORK_ENGINE_API TypedDataService {
public:
    bool isContainerValueType(const RuntimeValue& valueType) const;
    bool isStandardValueType(const RuntimeValue& valueType) const;
    bool shouldEvalValueType(const RuntimeValue& valueType) const;

    RuntimeValue getClassModulePath(const RuntimeValue& classReference) const;
    std::pair<RuntimeValue, RuntimeValue> getClassTypeMetadata(
        const RuntimeValue& classReference) const;
    RuntimeValue getAttrMetadata(const RuntimeValue& owner) const;
    RuntimeValue resolveAttrMetadata(const RuntimeValue& owner,
                                     const std::string& key) const;
    RuntimeValue resolveAttrValueType(const RuntimeValue& owner,
                                      const std::string& key) const;
    std::pair<RuntimeValue, RuntimeValue> resolveConfigVar(
        const RuntimeValue& owner, const std::string& key) const;
    std::pair<RuntimeValue, RuntimeValue> resolveMemberMetadata(
        const RuntimeValue& owner, const std::string& key) const;

    RuntimeValue evalDataExpression(
        const RuntimeValue& value,
        const RuntimeValue::Map& environment = RuntimeValue::Map{}) const;
    RuntimeValue coerceStandardValue(const RuntimeValue& value,
                                     const RuntimeValue& valueType) const;
    RuntimeValue resolveMetadataType(
        const RuntimeValue& typeReference,
        const std::string& declaringModule = std::string()) const;
    std::string metadataTypeName(const RuntimeValue& typeReference) const;
    RuntimeValue constructTypedValue(
        const RuntimeValue& value, const RuntimeValue& valueType,
        const std::string& declaringModule = std::string()) const;
    RuntimeValue resolveTypedDataValue(
        const RuntimeValue& value, const RuntimeValue& valueType,
        const RuntimeValue::Map& environment = RuntimeValue::Map{},
        const std::string& declaringModule = std::string()) const;

private:
    RuntimeValue unwrapOptional(const RuntimeValue& valueType) const;
    RuntimeValue coerceUnionValue(const RuntimeValue& value,
                                  const RuntimeValue::Array& arguments) const;
    bool matchesType(const RuntimeValue& value,
                     const RuntimeValue& valueType) const;
    RuntimeValue coerceBool(const RuntimeValue& value) const;
    RuntimeValue coerceInteger(const RuntimeValue& value) const;
    RuntimeValue coerceFloat(const RuntimeValue& value) const;
    RuntimeValue coerceContainer(const RuntimeValue& value) const;
};

LUDORK_ENGINE_API TypedDataService& typedDataService();

BIND_FUNCTION(name = "getClassModulePath")
RuntimeValue dataValueGetClassModulePath(const RuntimeValue& classReference);

BIND_FUNCTION(name = "getClassTypeMetadata", multiple_returns = true)
std::pair<RuntimeValue, RuntimeValue> dataValueGetClassTypeMetadata(
    const RuntimeValue& classReference);

BIND_FUNCTION(name = "getAttrMetadata")
RuntimeValue dataValueGetAttrMetadata(const RuntimeValue& owner);

BIND_FUNCTION(name = "resolveAttrMetadata")
RuntimeValue dataValueResolveAttrMetadata(const RuntimeValue& owner,
                                          const std::string& key);

BIND_FUNCTION(name = "resolveAttrValueType")
RuntimeValue dataValueResolveAttrValueType(const RuntimeValue& owner,
                                           const std::string& key);

BIND_FUNCTION(name = "resolveConfigVar", multiple_returns = true)
std::pair<RuntimeValue, RuntimeValue> dataValueResolveConfigVar(
    const RuntimeValue& owner, const std::string& key);

BIND_FUNCTION(name = "resolveMemberMetadata", multiple_returns = true)
std::pair<RuntimeValue, RuntimeValue> dataValueResolveMemberMetadata(
    const RuntimeValue& owner, const std::string& key);

BIND_FUNCTION(name = "isContainerValueType")
bool dataValueIsContainerValueType(const RuntimeValue& valueType);

BIND_FUNCTION(name = "isStandardValueType")
bool dataValueIsStandardValueType(const RuntimeValue& valueType);

BIND_FUNCTION(name = "shouldEvalValueType")
bool dataValueShouldEvalValueType(const RuntimeValue& valueType);

BIND_FUNCTION(name = "evalDataExpression", defaults = {nil})
RuntimeValue dataValueEvalDataExpression(
    const RuntimeValue& value,
    const RuntimeValue::Map& environment = RuntimeValue::Map{});

BIND_FUNCTION(name = "coerceStandardValue")
RuntimeValue dataValueCoerceStandardValue(const RuntimeValue& value,
                                          const RuntimeValue& valueType);

BIND_FUNCTION(name = "resolveMetadataType", defaults = {nil})
RuntimeValue dataValueResolveMetadataType(
    const RuntimeValue& typeReference,
    const std::string& declaringModule = std::string());

BIND_FUNCTION(name = "metadataTypeName")
std::string dataValueMetadataTypeName(const RuntimeValue& typeReference);

BIND_FUNCTION(name = "constructTypedValue", defaults = {nil})
RuntimeValue dataValueConstructTypedValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const std::string& declaringModule = std::string());

BIND_FUNCTION(name = "resolveTypedDataValue", defaults = {nil, nil})
RuntimeValue dataValueResolveTypedDataValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const RuntimeValue::Map& environment = RuntimeValue::Map{},
    const std::string& declaringModule = std::string());

BIND_FUNCTION(name = "GetConfigVars")
std::unordered_map<std::string, RuntimeValue> getConfigVars(
    const RuntimeValue& meta);
