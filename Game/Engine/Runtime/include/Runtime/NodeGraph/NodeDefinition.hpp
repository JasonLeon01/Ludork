#pragma once

#include <Runtime/RuntimeValue.hpp>
#include <Runtime/TypeSchema.hpp>
#include <RuntimeApi.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct NodeNamedValues {
    std::string name;
    RuntimeValue::Array values;
};

struct NodeMemberMetadata {
    std::vector<NodeNamedValues> execSplits;
    std::vector<NodeNamedValues> latentStates;
    bool latent = false;
    bool loop = false;
    bool pure = false;
    std::string loopNode;
    std::string kind;
};

class LUDORK_RUNTIME_API NodeDefinition {
public:
    struct CompiledParameter {
        CompiledParameter(std::string name, RuntimeValue type);

        const ludork::runtime::TypeSchema& schema() const;
        const RuntimeValue& typeReference() const;

        std::string name;
        std::optional<RuntimeValue> defaultValue;

    private:
        RuntimeValue type_;
        mutable std::optional<ludork::runtime::TypeSchema> schema_;
    };

    explicit NodeDefinition(const RuntimeValue& resolvedDefinition);
    explicit NodeDefinition(RuntimeIdentityPtr function);

    RuntimeIdentityPtr callable;
    std::vector<CompiledParameter> parameters;
    NodeMemberMetadata metadata;
    std::string declaringModule;
    std::string displayName;
    bool hasSelfParameter = false;

private:
    void parseMemberMetadata(RuntimeValueView value);
};
