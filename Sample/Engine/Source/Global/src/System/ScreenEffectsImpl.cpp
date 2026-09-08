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

namespace ludork::global::system_impl {

sf::Glsl::Vec4 FramePipelineImpl::makeToneColour(float red, float green,
                                                 float blue, float gray) {
    return {std::clamp(red, -255.0f, 255.0f) / 255.0f,
            std::clamp(green, -255.0f, 255.0f) / 255.0f,
            std::clamp(blue, -255.0f, 255.0f) / 255.0f,
            std::clamp(gray, 0.0f, 255.0f) / 255.0f};
}

sf::Glsl::Vec4 FramePipelineImpl::interpolateTone(const sf::Glsl::Vec4& start,
                                                  const sf::Glsl::Vec4& target,
                                                  float ratio) {
    const float amount = std::clamp(ratio, 0.0f, 1.0f);
    return {start.x + (target.x - start.x) * amount,
            start.y + (target.y - start.y) * amount,
            start.z + (target.z - start.z) * amount,
            start.w + (target.w - start.w) * amount};
}

bool FramePipelineImpl::isNeutralTone(const sf::Glsl::Vec4& colour) {
    return std::abs(colour.x) <= 0.0001f && std::abs(colour.y) <= 0.0001f &&
           std::abs(colour.z) <= 0.0001f && std::abs(colour.w) <= 0.0001f;
}

void FramePipelineImpl::applyScreenTonePass() {
    if (!shadersAvailable() || !toneActive_ || toneShader_ == nullptr ||
        canvas_ == nullptr || isNeutralTone(toneCurrentColour_)) {
        return;
    }
    canvas_->display();
    const sf::Vector2u size = canvas_->getSize();
    ensureToneBuffer(size);
    toneBufferSprite_->setTexture(canvas_->getTexture(), true);
    toneBufferSprite_->setPosition({0.0f, 0.0f});
    toneBufferSprite_->setScale({1.0f, 1.0f});
    toneShader_->setUniform("screenTex", canvas_->getTexture());
    toneShader_->setUniform(
        "texSize",
        sf::Vector2f{static_cast<float>(size.x), static_cast<float>(size.y)});
    applyScreenToneUniform();
    toneBuffer_->clear(sf::Color::Transparent);
    sf::RenderStates toneStates = canvasRenderStates();
    toneStates.shader = toneShader_.get();
    toneBuffer_->draw(*toneBufferSprite_, toneStates);
    toneBuffer_->display();
    const sf::View savedView = canvas_->getView();
    canvas_->clear(sf::Color::Transparent);
    canvas_->setView(canvas_->getDefaultView());
    toneBufferSprite_->setTexture(toneBuffer_->getTexture(), true);
    canvas_->draw(*toneBufferSprite_, canvasRenderStates());
    canvas_->setView(savedView);
}

void FramePipelineImpl::flashScreen(std::optional<sf::Color> color,
                                    float duration) {
    if (duration <= 0.0f) {
        stopFlash();
        return;
    }
    if (!shadersAvailable()) {
        warnOnce("System.flashScreen",
                 "Shaders are unavailable; skipped screen flash effect");
        return;
    }
    if (flashShader_ == nullptr) {
        try {
            flashShader_ =
                ShaderManager::load("/Game/Assets/Shaders/Global/Flash.frag");
        } catch (const std::exception&) {
            flashShader_.reset();
            std::cerr << "FLASH_SHADER_LOAD_FAILED\n";
            return;
        }
    }
    if (flashShader_ == nullptr) {
        return;
    }
    flashColour_ = sf::Glsl::Vec4(color.value_or(sf::Color::White));
    flashDuration_ = duration;
    flashTimeCount_ = 0.0f;
    if (!flashActive_) {
        addGraphicsShader(flashShader_);
        flashActive_ = true;
    }
    flashShader_->setUniform("flashColor", flashColour_);
    flashShader_->setUniform("intensity", 1.0f);
}

void FramePipelineImpl::stopFlash() {
    if (flashActive_ && flashShader_ != nullptr) {
        removeGraphicsShader(flashShader_);
    }
    flashActive_ = false;
    flashTimeCount_ = 0.0f;
    flashDuration_ = 0.0f;
}

bool FramePipelineImpl::isFlashing() {
    return flashActive_;
}

void FramePipelineImpl::changeScreenTone(float red, float green, float blue,
                                         float gray, float duration) {
    if (!shadersAvailable()) {
        warnOnce("System.changeScreenTone",
                 "Shaders are unavailable; skipped screen tone effect");
        return;
    }
    if (!ensureToneShader()) {
        return;
    }
    const sf::Glsl::Vec4 target = makeToneColour(red, green, blue, gray);
    toneStartColour_ = toneCurrentColour_;
    toneTargetColour_ = target;
    toneDuration_ = std::max(0.0f, duration);
    toneTimeCount_ = 0.0f;
    toneActive_ = true;
    if (toneDuration_ <= 0.0f) {
        toneCurrentColour_ = target;
        applyScreenToneUniform();
        if (isNeutralTone(target)) {
            stopScreenTone();
        }
    } else {
        applyScreenToneUniform();
    }
}

void FramePipelineImpl::clearScreenTone(float duration) {
    changeScreenTone(0.0f, 0.0f, 0.0f, 0.0f, duration);
}

void FramePipelineImpl::stopScreenTone() {
    toneCurrentColour_ = {};
    toneStartColour_ = {};
    toneTargetColour_ = {};
    toneDuration_ = 0.0f;
    toneTimeCount_ = 0.0f;
    toneActive_ = false;
}

bool FramePipelineImpl::isScreenToneActive() {
    return toneActive_;
}

bool FramePipelineImpl::isScreenToneTransitionComplete() {
    return !toneActive_ || toneDuration_ <= 0.0f;
}

void FramePipelineImpl::startShake(float power, float speed, float duration) {
    if (duration <= 0.0f) {
        stopShake();
        return;
    }
    shakePower_ = power;
    shakeSpeed_ = speed;
    shakeDuration_ = duration;
    shakeTimeCount_ = 0.0f;
    shakeActive_ = true;
    shakeNextUpdate_ = 0.0f;
    shakeOffset_ = {};
}

void FramePipelineImpl::stopShake() {
    shakeActive_ = false;
    shakeTimeCount_ = 0.0f;
    shakeDuration_ = 0.0f;
    shakeOffset_ = {};
}

bool FramePipelineImpl::isShaking() {
    return shakeActive_;
}

void FramePipelineImpl::updateFlash(float deltaTime) {
    if (!flashActive_ || flashShader_ == nullptr) {
        return;
    }
    flashTimeCount_ = std::min(flashTimeCount_ + deltaTime, flashDuration_);
    const float intensity =
        flashDuration_ > 0.0f
            ? std::max(0.0f, 1.0f - flashTimeCount_ / flashDuration_)
            : 0.0f;
    flashShader_->setUniform("flashColor", flashColour_);
    flashShader_->setUniform("intensity", intensity);
    if (flashTimeCount_ >= flashDuration_) {
        removeGraphicsShader(flashShader_);
        flashActive_ = false;
    }
}

void FramePipelineImpl::updateScreenTone(float deltaTime) {
    if (!toneActive_ || toneShader_ == nullptr) {
        return;
    }
    if (toneDuration_ > 0.0f) {
        toneTimeCount_ = std::min(toneTimeCount_ + deltaTime, toneDuration_);
        const float ratio = std::min(1.0f, toneTimeCount_ / toneDuration_);
        toneCurrentColour_ =
            interpolateTone(toneStartColour_, toneTargetColour_, ratio);
    }
    applyScreenToneUniform();
    if (toneDuration_ > 0.0f && toneTimeCount_ >= toneDuration_) {
        toneDuration_ = 0.0f;
        if (isNeutralTone(toneCurrentColour_)) {
            stopScreenTone();
        }
    }
}

void FramePipelineImpl::updateShake(float deltaTime) {
    if (!shakeActive_) {
        return;
    }
    shakeTimeCount_ = std::min(shakeTimeCount_ + deltaTime, shakeDuration_);
    if (shakeTimeCount_ >= shakeDuration_) {
        stopShake();
        return;
    }
    const float remainingPower =
        shakePower_ * (1.0f - shakeTimeCount_ / shakeDuration_);
    shakeNextUpdate_ -= deltaTime;
    if (shakeNextUpdate_ <= 0.0f) {
        shakeNextUpdate_ = shakeSpeed_ > 0.0f
                               ? 1.0f / shakeSpeed_
                               : std::numeric_limits<float>::max();
        std::uniform_real_distribution<float> offset(-remainingPower,
                                                     remainingPower);
        shakeOffset_ = {offset(random_), offset(random_)};
    }
}

bool FramePipelineImpl::ensureToneShader() {
    if (toneShader_ != nullptr) {
        return true;
    }
    try {
        toneShader_ =
            ShaderManager::load("/Game/Assets/Shaders/Global/Tone.frag");
    } catch (const std::exception&) {
        toneShader_.reset();
        std::cerr << "TONE_SHADER_LOAD_FAILED\n";
        return false;
    }
    return toneShader_ != nullptr;
}

void FramePipelineImpl::applyScreenToneUniform() {
    if (toneShader_ != nullptr) {
        toneShader_->setUniform("toneColor", toneCurrentColour_);
    }
}

void FramePipelineImpl::ensureToneBuffer(const sf::Vector2u& size) {
    if (toneBuffer_ == nullptr || toneBuffer_->getSize() != size) {
        toneBuffer_ = std::make_unique<sf::RenderTexture>(size);
        toneBufferSprite_.emplace(toneBuffer_->getTexture());
    } else if (!toneBufferSprite_.has_value()) {
        toneBufferSprite_.emplace(toneBuffer_->getTexture());
    }
}

}  // namespace ludork::global::system_impl
