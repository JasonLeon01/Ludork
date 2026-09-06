#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>
#include <Input/InputNamedValue.hpp>

enum class InputType {
    Mouse = 0,
    Gamepad = 1
};

using InputAxisComparison = std::function<bool(float, float)>;

enum class InputActionKind {
    KeyOrScan,
    Key,
    Scan,
    MouseButton,
    JoystickButton,
    JoystickAxis,
    TouchTap
};

BIND_MODULE_PROPERTY(name = "InputType", metadata = false, cache = true)
extern LUDORK_ENGINE_API const std::unordered_map<std::string, InputNamedValue>
    inputTypes;

BIND_MODULE_PROPERTY(name = "ActionKind", metadata = false)
extern LUDORK_ENGINE_API const std::unordered_map<std::string, int>
    inputActionKinds;

BIND_MODULE_PROPERTY(name = "AxisComparison", metadata = false)
extern LUDORK_ENGINE_API const
    std::unordered_map<std::string, InputAxisComparison>
        inputAxisComparisons;
