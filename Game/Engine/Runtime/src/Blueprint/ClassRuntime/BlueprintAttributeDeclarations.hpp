#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <string>

namespace ludork::runtime::class_runtime_detail {

struct BlueprintAttributeDeclarations {
    RuntimeHandle metadata;
    RuntimeHandle localMetadata;
    RuntimeHandle localTypes;
};

BlueprintAttributeDeclarations resolveBlueprintAttributeDeclarations(
    const RuntimeValue& parentClass, const RuntimeValue& rawDeclarations,
    const RuntimeValue& mixin, const std::string& scriptPath,
    const std::string& classPath);

void validateBlueprintAttributes(const RuntimeValue& attributes,
                                 const RuntimeHandle& metadata,
                                 const std::string& classPath);

}  // namespace ludork::runtime::class_runtime_detail
