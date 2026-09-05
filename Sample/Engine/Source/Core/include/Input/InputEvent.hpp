#pragma once

#include <CoreMinimal.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct InjectedInputEvent {
    BIND_PROPERTY()
    std::string type;

    BIND_PROPERTY()
    std::string key;

    BIND_PROPERTY()
    int scan = static_cast<int>(sf::Keyboard::Scancode::Unknown);

    BIND_PROPERTY()
    std::string button = "Left";

    BIND_PROPERTY()
    int x = 0;

    BIND_PROPERTY()
    int y = 0;

    BIND_PROPERTY()
    float delta = 0.0f;

    BIND_PROPERTY()
    bool alt = false;

    BIND_PROPERTY()
    bool control = false;

    BIND_PROPERTY()
    bool shift = false;

    BIND_PROPERTY()
    bool system = false;
};

BIND_CLASS(copyable = true)
struct JoystickAxisEvent {
    BIND_PROPERTY()
    sf::Joystick::Axis axis = sf::Joystick::Axis::X;

    BIND_PROPERTY()
    float position = 0.0f;
};
