#pragma once

#include <CoreMinimal.hpp>
#include <RuntimeApi.hpp>
#include <Runtime/NodeGraph/Types.hpp>

BIND_CLASS(copyable = true, table_init = true, metadata = false)
struct LUDORK_RUNTIME_API GraphLink {
    BIND_PROPERTY(metadata = false)
    RuntimeValue left;

    BIND_PROPERTY(metadata = false)
    RuntimeValue right;

    BIND_PROPERTY(metadata = false)
    int leftOutPin = 0;

    BIND_PROPERTY(metadata = false)
    int rightInPin = 0;

    BIND_PROPERTY(metadata = false)
    std::string linkType;
};
