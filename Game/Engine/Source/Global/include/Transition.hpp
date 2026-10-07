#pragma once

#include <Runtime/Async/AsyncOperation.hpp>

#include <CoreMinimal.hpp>

BIND_CLASS()
class Transition {
public:
    BIND_METHOD(nonnull_return = true, defaults = {nil, 1.0})
    static std::shared_ptr<AsyncOperation> setTransition(
        const std::shared_ptr<sf::Texture>& transitionResource = nullptr,
        float transitionTime = 1.0f);

    BIND_METHOD(nonnull_return = true)
    static std::shared_ptr<AsyncOperation> freezeTransitionBackground();

    BIND_METHOD(Pure = true)
    static bool isTransitionBackgroundFrozen();

    BIND_METHOD(Pure = true)
    static bool isTransitionBackgroundFreezePending();

    BIND_METHOD()
    static void cancelTransitionBackgroundFreeze();

    BIND_METHOD(nonnull_return = true, defaults = {nil, 1.0})
    static std::shared_ptr<AsyncOperation> requestTransition(
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
