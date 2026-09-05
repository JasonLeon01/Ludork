#pragma once

#include <CoreMinimal.hpp>

#include <RuntimeApi.hpp>

BIND_CLASS(callbacks = true)
class LUDORK_RUNTIME_API Component : public RuntimeObject {
public:
    BIND_INIT()
    Component() = default;

    virtual ~Component() = default;

    BIND_METHOD()
    virtual RuntimeValue::Array onAttach(const RuntimeIdentityPtr& owner);
};
