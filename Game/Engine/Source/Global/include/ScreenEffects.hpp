#pragma once

#include <CoreMinimal.hpp>

BIND_CLASS()
class ScreenEffects {
public:
    static void applyScreenTonePass();

    BIND_METHOD(defaults = {nil, 0.5})
    static void flashScreen(std::optional<sf::Color> color = std::nullopt,
                            float duration = 0.5f);

    BIND_METHOD()
    static void stopFlash();

    BIND_METHOD(Pure = true)
    static bool isFlashing();

    BIND_METHOD(defaults = {0.0, 0.0, 0.0, 0.0, 0.0})
    static void changeScreenTone(float red = 0.0f, float green = 0.0f,
                                 float blue = 0.0f, float gray = 0.0f,
                                 float duration = 0.0f);

    BIND_METHOD(defaults = {0.0})
    static void clearScreenTone(float duration = 0.0f);

    BIND_METHOD()
    static void stopScreenTone();

    BIND_METHOD(Pure = true)
    static bool isScreenToneActive();

    BIND_METHOD(Pure = true)
    static bool isScreenToneTransitionComplete();

    BIND_METHOD(defaults = {4.0, 10.0, 0.5})
    static void startShake(float power = 4.0f, float speed = 10.0f,
                           float duration = 0.5f);

    BIND_METHOD()
    static void stopShake();

    BIND_METHOD(Pure = true)
    static bool isShaking();
};
