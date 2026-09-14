#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <RuntimeApi.hpp>

BIND_CLASS(bind_bases = false, cast_bases = {"RuntimeObject"}, metadata = false)
class LUDORK_RUNTIME_API DataNode : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(DataNode, RuntimeObject)

    BIND_INIT()
    DataNode(std::string nodeFunction, RuntimeValue params,
             RuntimeValue resolvedDefinition);

    ~DataNode() override = default;

    BIND_PROPERTY(metadata = false)
    std::string nodeFunction;

    BIND_PROPERTY(metadata = false)
    RuntimeValue params;

    BIND_PROPERTY(metadata = false)
    RuntimeValue position;

    RuntimeValue::Array getParams() const;

    const RuntimeValue& getResolvedDefinition() const;

private:
    RuntimeValue resolvedDefinition_;
};
