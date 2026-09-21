#pragma once

#include <CoreMinimal.hpp>

BIND_CLASS()
class Transition {
public:
    BIND_METHOD(defaults = {nil, 1.0})
    static void setTransition(
        const std::shared_ptr<sf::Texture>& transitionResource = nullptr,
        float transitionTime = 1.0f);

    BIND_METHOD()
    static void freezeTransitionBackground();

    BIND_METHOD(Pure = true)
    static bool isTransitionBackgroundFrozen();

    BIND_METHOD(Pure = true)
    static bool isTransitionBackgroundFreezePending();

    BIND_METHOD()
    static void cancelTransitionBackgroundFreeze();

    BIND_METHOD(defaults = {nil, 1.0})
    static void requestTransition(
        std::optional<std::string> transitionName = std::nullopt,
        float transitionTime = 1.0f);

    BIND_METHOD()
    static void cancelPendingTransition();

    BIND_METHOD(Pure = true)
    static bool isTransitionPending();

    BIND_METHOD(Pure = true)
    static bool isInTransition();

    static void applyPendingTransition();
};
