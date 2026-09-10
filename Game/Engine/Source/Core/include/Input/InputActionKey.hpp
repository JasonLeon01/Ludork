#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <Input/InputAction.hpp>

BIND_CLASS(
    copyable = true, table_init = true,
    lua_alternatives =
        "number=>kind=InputActionKind::KeyOrScan,code=$;fields(name,value)=>"
        "kind=InputActionKind::JoystickButton,name=$name,code=$value;array("
        "axis,threshold,comparison)=>kind=InputActionKind::JoystickAxis,code=$"
        "axis,threshold=$threshold,comparison=$comparison,comparisonIdentity=$"
        "comparison",
    lua_emit =
        "kind=InputActionKind::KeyOrScan=>value($code);kind=InputActionKind::"
        "Key=>fields(kind=$kind,code=$code);kind=InputActionKind::Scan=>fields("
        "kind=$kind,code=$code);kind=InputActionKind::JoystickButton=>fields("
        "name=$name,value=$code);kind=InputActionKind::JoystickAxis=>array($"
        "code,$threshold,$comparison)")
struct InputActionKey {
    BIND_PROPERTY()
    InputActionKind kind = InputActionKind::KeyOrScan;

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    int code = 0;

    BIND_PROPERTY()
    float threshold = 0.0f;

    BIND_PROPERTY(metadata = false)
    InputAxisComparison comparison;

    BIND_PROPERTY(metadata = false)
    RuntimeIdentityPtr comparisonIdentity;

    bool operator==(const InputActionKey& other) const;
};
