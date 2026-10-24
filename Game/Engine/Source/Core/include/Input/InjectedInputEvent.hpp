#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct InjectedInputEvent {
    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string KEY_PRESSED;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string KEY_RELEASED;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string MOUSE_MOVED;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string MOUSE_BUTTON_PRESSED;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string MOUSE_BUTTON_RELEASED;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string MOUSE_WHEEL_SCROLLED;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string MOUSE_ENTERED;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string MOUSE_LEFT;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string TEXT_ENTERED;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string TEXT_PREEDIT;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string TEXT_COMPOSITION;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string FOCUS_GAINED;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string FOCUS_LOST;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string BUTTON_LEFT;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string BUTTON_RIGHT;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string BUTTON_MIDDLE;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string BUTTON_EXTRA1;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string BUTTON_EXTRA2;

    BIND_PROPERTY()
    std::string type;

    BIND_PROPERTY()
    std::string key;

    BIND_PROPERTY()
    int scan = static_cast<int>(sf::Keyboard::Scancode::Unknown);

    BIND_PROPERTY(default = "Left")
    std::string button = BUTTON_LEFT;

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

    BIND_PROPERTY()
    std::string session;

    BIND_PROPERTY()
    int unicode = 0;

    BIND_PROPERTY()
    std::string text;

    BIND_PROPERTY()
    int preeditCaret = 0;

    BIND_PROPERTY()
    bool composing = false;
};
