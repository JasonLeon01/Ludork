#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true)
struct JoystickAxisEvent {
    BIND_PROPERTY()
    sf::Joystick::Axis axis = sf::Joystick::Axis::X;

    BIND_PROPERTY()
    float position = 0.0f;
};
