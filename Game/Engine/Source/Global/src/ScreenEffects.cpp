#include <ScreenEffects.hpp>
#include "System/FramePipelineImpl.hpp"
#include <utility>

void ScreenEffects::applyScreenTonePass() {
    ludork::global::system_impl::framePipelineImpl()
        .screenEffects()
        .applyScreenTonePass(
            ludork::global::system_impl::framePipelineImpl().getCanvas());
}

void ScreenEffects::flashScreen(std::optional<sf::Color> color,
                                float duration) {
    ludork::global::system_impl::framePipelineImpl()
        .screenEffects()
        .flashScreen(std::move(color), duration);
}

void ScreenEffects::stopFlash() {
    ludork::global::system_impl::framePipelineImpl()
        .screenEffects()
        .stopFlash();
}

bool ScreenEffects::isFlashing() {
    return ludork::global::system_impl::framePipelineImpl()
        .screenEffects()
        .isFlashing();
}

void ScreenEffects::changeScreenTone(float red, float green, float blue,
                                     float gray, float duration) {
    ludork::global::system_impl::framePipelineImpl()
        .screenEffects()
        .changeScreenTone(red, green, blue, gray, duration);
}

void ScreenEffects::clearScreenTone(float duration) {
    ludork::global::system_impl::framePipelineImpl()
        .screenEffects()
        .clearScreenTone(duration);
}

void ScreenEffects::stopScreenTone() {
    ludork::global::system_impl::framePipelineImpl()
        .screenEffects()
        .stopScreenTone();
}

bool ScreenEffects::isScreenToneActive() {
    return ludork::global::system_impl::framePipelineImpl()
        .screenEffects()
        .isScreenToneActive();
}

bool ScreenEffects::isScreenToneTransitionComplete() {
    return ludork::global::system_impl::framePipelineImpl()
        .screenEffects()
        .isScreenToneTransitionComplete();
}

void ScreenEffects::startShake(float power, float speed, float duration) {
    ludork::global::system_impl::framePipelineImpl().screenEffects().startShake(
        power, speed, duration);
}

void ScreenEffects::stopShake() {
    ludork::global::system_impl::framePipelineImpl()
        .screenEffects()
        .stopShake();
}

bool ScreenEffects::isShaking() {
    return ludork::global::system_impl::framePipelineImpl()
        .screenEffects()
        .isShaking();
}
