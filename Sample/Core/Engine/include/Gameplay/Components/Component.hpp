#pragma once

#include <BindAnnotations.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <vector>

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API Component : public RuntimeObject {
public:
    BIND_INIT()
    Component() = default;

    virtual ~Component() = default;

    BIND_METHOD()
    virtual RuntimeValue::Array onAttach(const RuntimeIdentityPtr& owner);
};
