#pragma once

#include <RuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <string>
#include <utility>
#include <optional>

class LUDORK_RUNTIME_API MetadataRuntimeFacade {
public:
    RuntimeValue::Map configVars(const RuntimeValue& metadata) const;
    std::optional<std::string> classModulePath(
        const RuntimeValue& classReference) const;
    std::pair<RuntimeValue, RuntimeValue> classTypeMetadata(
        const RuntimeValue& classReference) const;
    RuntimeValue attrMetadata(const RuntimeValue& owner) const;
    RuntimeValue resolveAttrMetadata(const RuntimeValue& owner,
                                     const std::string& key) const;
    RuntimeValue resolveAttrValueType(const RuntimeValue& owner,
                                      const std::string& key) const;
    std::pair<RuntimeValue, RuntimeValue> resolveConfigVar(
        const RuntimeValue& owner, const std::string& key) const;
    std::pair<RuntimeValue, RuntimeValue> resolveMemberMetadata(
        const RuntimeValue& owner, const std::string& key) const;
    RuntimeValue evaluateExpression(
        const RuntimeValue& value,
        const RuntimeValue::Map& environment = RuntimeValue::Map{}) const;
    RuntimeValue resolveType(
        const RuntimeValue& typeReference,
        const std::string& declaringModule = std::string()) const;
    RuntimeValue constructTypedValue(
        const RuntimeValue& value, const RuntimeValue& valueType,
        const std::string& declaringModule = std::string()) const;
};

LUDORK_RUNTIME_API MetadataRuntimeFacade& metadataRuntime();
