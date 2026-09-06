#pragma once

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct GameplayModifier {
    BIND_PROPERTY()
    std::string attribute;

    BIND_PROPERTY()
    std::string operation;

    BIND_PROPERTY(type = any)
    RuntimeValue magnitude;

    BIND_PROPERTY()
    RuntimeValue minimum;
};
