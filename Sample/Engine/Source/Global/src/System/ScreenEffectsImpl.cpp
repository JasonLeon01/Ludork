#include "ScreenEffectsImpl.hpp"
#include "SystemImpl.hpp"
#include "FramePipelineImpl.hpp"
#include <Manager/ShaderManager.hpp>
#include <Utils/Inner.hpp>
#include <Utils/Render.hpp>
#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <random>

namespace ludork::global::system_screen_effects_impl {

sf::Glsl::Vec4 makeToneColour(float red, float green, float blue, float gray) {
    return {std::clamp(red, -255.0f, 255.0f) / 255.0f,
            std::clamp(green, -255.0f, 255.0f) / 255.0f,
            std::clamp(blue, -255.0f, 255.0f) / 255.0f,
            std::clamp(gray, 0.0f, 255.0f) / 255.0f};
}

sf::Glsl::Vec4 interpolateTone(const sf::Glsl::Vec4& start,
                               const sf::Glsl::Vec4& target, float ratio) {
    const float amount = std::clamp(ratio, 0.0f, 1.0f);
    return {start.x + (target.x - start.x) * amount,
            start.y + (target.y - start.y) * amount,
            start.z + (target.z - start.z) * amount,
            start.w + (target.w - start.w) * amount};
}

bool isNeutralTone(const sf::Glsl::Vec4& colour) {
    return std::abs(colour.x) <= 0.0001f && std::abs(colour.y) <= 0.0001f &&
           std::abs(colour.z) <= 0.0001f && std::abs(colour.w) <= 0.0001f;
}

void applyScreenTonePass() {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::DisplayImpl& display = systemImpl.display;
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        systemImpl.framePipeline;
    if (!ludork::global::system_frame_pipeline_impl::shadersAvailable() ||
        !framePipeline.toneActive_ || framePipeline.toneShader_ == nullptr ||
        display.canvas_ == nullptr ||
        ludork::global::system_screen_effects_impl::isNeutralTone(
            framePipeline.toneCurrentColour_)) {
        return;
    }
    display.canvas_->display();
    const sf::Vector2u size = display.canvas_->getSize();
    ludork::global::system_screen_effects_impl::ensureToneBuffer(size);
    framePipeline.toneBufferSprite_->setTexture(display.canvas_->getTexture(),
                                                true);
    framePipeline.toneBufferSprite_->setPosition({0.0f, 0.0f});
    framePipeline.toneBufferSprite_->setScale({1.0f, 1.0f});
    framePipeline.toneShader_->setUniform("screenTex",
                                          display.canvas_->getTexture());
    framePipeline.toneShader_->setUniform(
        "texSize",
        sf::Vector2f{static_cast<float>(size.x), static_cast<float>(size.y)});
    ludork::global::system_screen_effects_impl::applyScreenToneUniform();
    framePipeline.toneBuffer_->clear(sf::Color::Transparent);
    sf::RenderStates toneStates = canvasRenderStates();
    toneStates.shader = framePipeline.toneShader_.get();
    framePipeline.toneBuffer_->draw(*framePipeline.toneBufferSprite_,
                                    toneStates);
    framePipeline.toneBuffer_->display();
    const sf::View savedView = display.canvas_->getView();
    display.canvas_->clear(sf::Color::Transparent);
    display.canvas_->setView(display.canvas_->getDefaultView());
    framePipeline.toneBufferSprite_->setTexture(
        framePipeline.toneBuffer_->getTexture(), true);
    display.canvas_->draw(*framePipeline.toneBufferSprite_,
                          canvasRenderStates());
    display.canvas_->setView(savedView);
}

void flashScreen(std::optional<sf::Color> color, float duration) {
    if (duration <= 0.0f) {
        ludork::global::system_screen_effects_impl::stopFlash();
        return;
    }
    if (!ludork::global::system_frame_pipeline_impl::shadersAvailable()) {
        warnOnce("System.flashScreen",
                 "Shaders are unavailable; skipped screen flash effect");
        return;
    }
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    if (framePipeline.flashShader_ == nullptr) {
        try {
            framePipeline.flashShader_ =
                ShaderManager::load("/Game/Assets/Shaders/Global/Flash.frag");
        } catch (const std::exception&) {
            framePipeline.flashShader_.reset();
            std::cerr << "FLASH_SHADER_LOAD_FAILED\n";
            return;
        }
    }
    if (framePipeline.flashShader_ == nullptr) {
        return;
    }
    framePipeline.flashColour_ =
        sf::Glsl::Vec4(color.value_or(sf::Color::White));
    framePipeline.flashDuration_ = duration;
    framePipeline.flashTimeCount_ = 0.0f;
    if (!framePipeline.flashActive_) {
        ludork::global::system_frame_pipeline_impl::addGraphicsShader(
            framePipeline.flashShader_);
        framePipeline.flashActive_ = true;
    }
    framePipeline.flashShader_->setUniform("flashColor",
                                           framePipeline.flashColour_);
    framePipeline.flashShader_->setUniform("intensity", 1.0f);
}

void stopFlash() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    if (framePipeline.flashActive_ && framePipeline.flashShader_ != nullptr) {
        ludork::global::system_frame_pipeline_impl::removeGraphicsShader(
            framePipeline.flashShader_);
    }
    framePipeline.flashActive_ = false;
    framePipeline.flashTimeCount_ = 0.0f;
    framePipeline.flashDuration_ = 0.0f;
}

bool isFlashing() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    return framePipeline.flashActive_;
}

void changeScreenTone(float red, float green, float blue, float gray,
                      float duration) {
    if (!ludork::global::system_frame_pipeline_impl::shadersAvailable()) {
        warnOnce("System.changeScreenTone",
                 "Shaders are unavailable; skipped screen tone effect");
        return;
    }
    if (!ludork::global::system_screen_effects_impl::ensureToneShader()) {
        return;
    }
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    const sf::Glsl::Vec4 target =
        ludork::global::system_screen_effects_impl::makeToneColour(red, green,
                                                                   blue, gray);
    framePipeline.toneStartColour_ = framePipeline.toneCurrentColour_;
    framePipeline.toneTargetColour_ = target;
    framePipeline.toneDuration_ = std::max(0.0f, duration);
    framePipeline.toneTimeCount_ = 0.0f;
    framePipeline.toneActive_ = true;
    if (framePipeline.toneDuration_ <= 0.0f) {
        framePipeline.toneCurrentColour_ = target;
        ludork::global::system_screen_effects_impl::applyScreenToneUniform();
        if (ludork::global::system_screen_effects_impl::isNeutralTone(target)) {
            ludork::global::system_screen_effects_impl::stopScreenTone();
        }
    } else {
        ludork::global::system_screen_effects_impl::applyScreenToneUniform();
    }
}

void clearScreenTone(float duration) {
    ludork::global::system_screen_effects_impl::changeScreenTone(
        0.0f, 0.0f, 0.0f, 0.0f, duration);
}

void stopScreenTone() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    framePipeline.toneCurrentColour_ = {};
    framePipeline.toneStartColour_ = {};
    framePipeline.toneTargetColour_ = {};
    framePipeline.toneDuration_ = 0.0f;
    framePipeline.toneTimeCount_ = 0.0f;
    framePipeline.toneActive_ = false;
}

bool isScreenToneActive() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    return framePipeline.toneActive_;
}

bool isScreenToneTransitionComplete() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    return !framePipeline.toneActive_ || framePipeline.toneDuration_ <= 0.0f;
}

void startShake(float power, float speed, float duration) {
    if (duration <= 0.0f) {
        ludork::global::system_screen_effects_impl::stopShake();
        return;
    }
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    framePipeline.shakePower_ = power;
    framePipeline.shakeSpeed_ = speed;
    framePipeline.shakeDuration_ = duration;
    framePipeline.shakeTimeCount_ = 0.0f;
    framePipeline.shakeActive_ = true;
    framePipeline.shakeNextUpdate_ = 0.0f;
    framePipeline.shakeOffset_ = {};
}

void stopShake() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    framePipeline.shakeActive_ = false;
    framePipeline.shakeTimeCount_ = 0.0f;
    framePipeline.shakeDuration_ = 0.0f;
    framePipeline.shakeOffset_ = {};
}

bool isShaking() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    return framePipeline.shakeActive_;
}

void updateFlash(float deltaTime) {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    if (!framePipeline.flashActive_ || framePipeline.flashShader_ == nullptr) {
        return;
    }
    framePipeline.flashTimeCount_ =
        std::min(framePipeline.flashTimeCount_ + deltaTime,
                 framePipeline.flashDuration_);
    const float intensity =
        framePipeline.flashDuration_ > 0.0f
            ? std::max(0.0f, 1.0f - framePipeline.flashTimeCount_ /
                                        framePipeline.flashDuration_)
            : 0.0f;
    framePipeline.flashShader_->setUniform("flashColor",
                                           framePipeline.flashColour_);
    framePipeline.flashShader_->setUniform("intensity", intensity);
    if (framePipeline.flashTimeCount_ >= framePipeline.flashDuration_) {
        ludork::global::system_frame_pipeline_impl::removeGraphicsShader(
            framePipeline.flashShader_);
        framePipeline.flashActive_ = false;
    }
}

void updateScreenTone(float deltaTime) {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    if (!framePipeline.toneActive_ || framePipeline.toneShader_ == nullptr) {
        return;
    }
    if (framePipeline.toneDuration_ > 0.0f) {
        framePipeline.toneTimeCount_ =
            std::min(framePipeline.toneTimeCount_ + deltaTime,
                     framePipeline.toneDuration_);
        const float ratio = std::min(
            1.0f, framePipeline.toneTimeCount_ / framePipeline.toneDuration_);
        framePipeline.toneCurrentColour_ =
            ludork::global::system_screen_effects_impl::interpolateTone(
                framePipeline.toneStartColour_, framePipeline.toneTargetColour_,
                ratio);
    }
    ludork::global::system_screen_effects_impl::applyScreenToneUniform();
    if (framePipeline.toneDuration_ > 0.0f &&
        framePipeline.toneTimeCount_ >= framePipeline.toneDuration_) {
        framePipeline.toneDuration_ = 0.0f;
        if (ludork::global::system_screen_effects_impl::isNeutralTone(
                framePipeline.toneCurrentColour_)) {
            ludork::global::system_screen_effects_impl::stopScreenTone();
        }
    }
}

void updateShake(float deltaTime) {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    if (!framePipeline.shakeActive_) {
        return;
    }
    framePipeline.shakeTimeCount_ =
        std::min(framePipeline.shakeTimeCount_ + deltaTime,
                 framePipeline.shakeDuration_);
    if (framePipeline.shakeTimeCount_ >= framePipeline.shakeDuration_) {
        ludork::global::system_screen_effects_impl::stopShake();
        return;
    }
    const float remainingPower =
        framePipeline.shakePower_ *
        (1.0f - framePipeline.shakeTimeCount_ / framePipeline.shakeDuration_);
    framePipeline.shakeNextUpdate_ -= deltaTime;
    if (framePipeline.shakeNextUpdate_ <= 0.0f) {
        framePipeline.shakeNextUpdate_ =
            framePipeline.shakeSpeed_ > 0.0f
                ? 1.0f / framePipeline.shakeSpeed_
                : std::numeric_limits<float>::max();
        std::uniform_real_distribution<float> offset(-remainingPower,
                                                     remainingPower);
        framePipeline.shakeOffset_ = {offset(framePipeline.random_),
                                      offset(framePipeline.random_)};
    }
}

bool ensureToneShader() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    if (framePipeline.toneShader_ != nullptr) {
        return true;
    }
    try {
        framePipeline.toneShader_ =
            ShaderManager::load("/Game/Assets/Shaders/Global/Tone.frag");
    } catch (const std::exception&) {
        framePipeline.toneShader_.reset();
        std::cerr << "TONE_SHADER_LOAD_FAILED\n";
        return false;
    }
    return framePipeline.toneShader_ != nullptr;
}

void applyScreenToneUniform() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    if (framePipeline.toneShader_ != nullptr) {
        framePipeline.toneShader_->setUniform("toneColor",
                                              framePipeline.toneCurrentColour_);
    }
}

void ensureToneBuffer(const sf::Vector2u& size) {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    if (framePipeline.toneBuffer_ == nullptr ||
        framePipeline.toneBuffer_->getSize() != size) {
        framePipeline.toneBuffer_ = std::make_unique<sf::RenderTexture>(size);
        framePipeline.toneBufferSprite_.emplace(
            framePipeline.toneBuffer_->getTexture());
    } else if (!framePipeline.toneBufferSprite_.has_value()) {
        framePipeline.toneBufferSprite_.emplace(
            framePipeline.toneBuffer_->getTexture());
    }
}

}  // namespace ludork::global::system_screen_effects_impl
