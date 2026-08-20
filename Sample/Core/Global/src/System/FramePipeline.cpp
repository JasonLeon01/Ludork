#include <System.hpp>

#include "Diagnostics/PerformanceProfiler.hpp"
#include "Platform/NativeDisplay.hpp"
#include "Platform/NativeInputMethod.hpp"
#include "SystemRuntime.hpp"

#include <Fog/FogController.hpp>
#include <Manager/AssetPath.hpp>
#include <Manager/ShaderManager.hpp>
#include <Manager/TextureManager.hpp>
#include <Utils/Inner.hpp>
#include <Utils/Render.hpp>
#include <Weather/WeatherController.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>
#include <utility>

void System::setWeather(const RuntimeValue& weatherType, float power,
                        int maxCount) {
    WeatherController::setWeather(weatherType, power, maxCount);
}

void System::clearWeather() {
    WeatherController::clearWeather();
}

void System::updateWeather(float deltaTime) {
    WeatherController::update(deltaTime);
}

void System::updateFog(float deltaTime) {
    FogController::update(deltaTime);
}

void System::clearFog() {
    FogController::clearFog();
}

void System::applyFogFromMapData(const RuntimeValue::Map& mapData) {
    FogController::applyFromMapData(mapData);
}

void System::draw(const sf::Drawable& drawable, sf::Shader* shader) {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.canvas_ == nullptr) {
        return;
    }
    sf::RenderStates states = canvasRenderStates();
    states.shader = shader;
    display.canvas_->draw(drawable, states);
}

void System::applyScreenTonePass() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    if (!shadersAvailable() || !framePipeline.toneActive_ ||
        framePipeline.toneShader_ == nullptr || display.canvas_ == nullptr ||
        isNeutralTone(framePipeline.toneCurrentColour_)) {
        return;
    }
    display.canvas_->display();
    const sf::Vector2u size = display.canvas_->getSize();
    ensureToneBuffer(size);
    framePipeline.toneBufferSprite_->setTexture(display.canvas_->getTexture(),
                                                true);
    framePipeline.toneBufferSprite_->setPosition({0.0f, 0.0f});
    framePipeline.toneBufferSprite_->setScale({1.0f, 1.0f});
    framePipeline.toneShader_->setUniform("screenTex",
                                          display.canvas_->getTexture());
    framePipeline.toneShader_->setUniform(
        "texSize",
        sf::Vector2f{static_cast<float>(size.x), static_cast<float>(size.y)});
    applyScreenToneUniform();
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

void System::composeFrame(float deltaTime) {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    framePipeline.transitionCompletionPending_ = false;
    if (display.window_ == nullptr || display.canvas_ == nullptr ||
        !display.canvasSprite_.has_value()) {
        return;
    }
    if (framePipeline.inTransition_) {
        framePipeline.transitionTimeCount_ =
            std::min(framePipeline.transitionTimeCount_ + deltaTime,
                     framePipeline.transitionTime_);
    }
    updateFlash(deltaTime);
    updateScreenTone(deltaTime);
    updateShake(deltaTime);
    updateWeather(deltaTime);
    updateFog(deltaTime);
    applyPendingTransition();
    display.canvas_->display();
    sf::RenderTexture* finalCanvas = display.canvas_.get();
    for (std::size_t index = 0; index < framePipeline.graphicsCanvases_.size();
         ++index) {
        sf::RenderTexture& target = *framePipeline.graphicsCanvases_[index];
        sf::RenderTexture& source =
            index == 0 ? *display.canvas_
                       : *framePipeline.graphicsCanvases_[index - 1];
        target.clear(sf::Color::Transparent);
        sf::Sprite sprite(source.getTexture());
        sf::RenderStates states = canvasRenderStates();
        const std::shared_ptr<sf::Shader>& shader =
            framePipeline.graphicsShaders_[index];
        if (shader != nullptr) {
            shader->setUniform("screenTex", source.getTexture());
            const sf::Vector2u textureSize = source.getTexture().getSize();
            shader->setUniform("texSize",
                               sf::Vector2f{static_cast<float>(textureSize.x),
                                            static_cast<float>(textureSize.y)});
            states.shader = shader.get();
        }
        target.draw(sprite, states);
        target.display();
        finalCanvas = &target;
    }
    display.canvasSprite_->setTexture(finalCanvas->getTexture(), true);
    if (framePipeline.shakeActive_) {
        const sf::Vector2u textureSize = finalCanvas->getSize();
        if (textureSize.x > 0 && textureSize.y > 0) {
            const float pad = framePipeline.shakePower_;
            display.canvasSprite_->setScale(
                {(static_cast<float>(textureSize.x) + pad * 2.0f) /
                     static_cast<float>(textureSize.x),
                 (static_cast<float>(textureSize.y) + pad * 2.0f) /
                     static_cast<float>(textureSize.y)});
            display.canvasSprite_->setPosition(
                {-pad + framePipeline.shakeOffset_.x,
                 -pad + framePipeline.shakeOffset_.y});
        }
    }
    if (framePipeline.transitionOutputTexture_ == nullptr ||
        !framePipeline.transitionOutputSprite_.has_value()) {
        display.window_->draw(*display.canvasSprite_, canvasRenderStates());
    } else if (framePipeline.inTransition_ &&
               framePipeline.transitionShader_ != nullptr &&
               framePipeline.transition_ != nullptr &&
               framePipeline.transitionTempTexture_ != nullptr &&
               framePipeline.transitionSprite_.has_value()) {
        framePipeline.transitionTempTexture_->clear(sf::Color::Transparent);
        framePipeline.transitionTempTexture_->draw(*display.canvasSprite_,
                                                   sf::BlendNone);
        framePipeline.transitionTempTexture_->display();
        framePipeline.transitionShader_->setUniform(
            "screenTex", framePipeline.transitionTempTexture_->getTexture());
        framePipeline.transitionShader_->setUniform(
            "backTex", framePipeline.transition_->getTexture());
        framePipeline.transitionShader_->setUniform(
            "transitionResource",
            framePipeline.transitionResource_ != nullptr &&
                    framePipeline.transitionMaskTexture_ != nullptr
                ? framePipeline.transitionMaskTexture_->getTexture()
                : framePipeline.transition_->getTexture());
        framePipeline.transitionShader_->setUniform(
            "useMask", framePipeline.transitionResource_ != nullptr &&
                           framePipeline.transitionMaskTexture_ != nullptr);
        framePipeline.transitionShader_->setUniform(
            "progress", framePipeline.transitionTimeCount_);
        framePipeline.transitionShader_->setUniform(
            "totalTime", framePipeline.transitionTime_);
        sf::RenderStates states(sf::BlendNone);
        states.shader = framePipeline.transitionShader_.get();
        framePipeline.transitionOutputTexture_->clear(sf::Color::Transparent);
        framePipeline.transitionOutputTexture_->draw(
            *framePipeline.transitionSprite_, states);
        framePipeline.transitionOutputTexture_->display();
        display.window_->draw(*framePipeline.transitionOutputSprite_,
                              canvasRenderStates());
    } else {
        framePipeline.transitionOutputTexture_->clear(sf::Color::Transparent);
        framePipeline.transitionOutputTexture_->draw(*display.canvasSprite_,
                                                     sf::BlendNone);
        framePipeline.transitionOutputTexture_->display();
        display.window_->draw(*framePipeline.transitionOutputSprite_,
                              canvasRenderStates());
    }
    if (framePipeline.shakeActive_) {
        display.canvasSprite_->setScale({1.0f, 1.0f});
        display.canvasSprite_->setPosition({0.0f, 0.0f});
    }
    if (framePipeline.transitionFreezePending_) {
        cacheTransitionBackground();
        framePipeline.transitionFreezePending_ = false;
        framePipeline.transitionFrozen_ = true;
    }
    framePipeline.composedTransitionRevision_ =
        framePipeline.transitionRevision_;
    framePipeline.transitionCompletionPending_ =
        framePipeline.inTransition_ &&
        framePipeline.transitionTimeCount_ >= framePipeline.transitionTime_;
}

void System::present() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.presentMutex_);
    if (display.window_ == nullptr || display.canvas_ == nullptr ||
        !display.canvasSprite_.has_value()) {
        return;
    }
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
    if (!isEmbeddedDisplay() && !display.desktopFullscreen_) {
        const sf::Vector2u windowSize = display.window_->getSize();
        const bool liveResizing = ludork::global::isNativeWindowLiveResizing(
            display.window_->getNativeHandle());
        if (liveResizing || windowSize != display.observedWindowSize_) {
            const std::optional<sf::Vector2u> clientSize =
                ludork::global::getWindowedClientSize(
                    display.window_->getNativeHandle());
            if (liveResizing || !clientSize.has_value() ||
                clientSize != display.observedWindowClientSize_) {
                display.pendingResizeScale_ =
                    windowFitScale(clientSize.value_or(windowSize));
                display.lastResizeTime_ = std::chrono::steady_clock::now();
            }
            return;
        }
        if (display.pendingResizeScale_.has_value()) {
            return;
        }
    }
#endif
    if (PerformanceProfiler::isEnabled()) {
        const auto presentStart = std::chrono::steady_clock::now();
        display.window_->display();
        PerformanceProfiler::addPresentWait(
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - presentStart)
                .count());
    } else {
        display.window_->display();
    }
}

void System::completeFrame() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    if (display.window_ == nullptr || display.canvas_ == nullptr ||
        !display.canvasSprite_.has_value()) {
        framePipeline.transitionCompletionPending_ = false;
        return;
    }
    if (framePipeline.transitionCompletionPending_ &&
        framePipeline.composedTransitionRevision_ ==
            framePipeline.transitionRevision_) {
        framePipeline.inTransition_ = false;
    }
    framePipeline.transitionCompletionPending_ = false;
    applyPendingDisplayChanges();
}

void System::addGraphicsShader(const std::shared_ptr<sf::Shader>& shader,
                               std::optional<ShaderUniforms> uniforms) {
    if (!shadersAvailable()) {
        if (shader != nullptr) {
            warnOnce("System.addGraphicsShader",
                     "Shaders are unavailable; ignored addGraphicsShader");
        }
        return;
    }
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.graphicsShaders_.push_back(shader);
    if (shader != nullptr && uniforms.has_value()) {
        for (const auto& [name, value] : *uniforms) {
            setShaderUniform(*shader, name, value);
        }
    }
    applyGraphicsShadersLength();
}

void System::removeGraphicsShader(const std::shared_ptr<sf::Shader>& shader) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    const auto iterator =
        std::find(framePipeline.graphicsShaders_.begin(),
                  framePipeline.graphicsShaders_.end(), shader);
    if (iterator != framePipeline.graphicsShaders_.end()) {
        framePipeline.graphicsShaders_.erase(iterator);
    }
    applyGraphicsShadersLength();
}

void System::removeAllGraphicsShaders() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.graphicsShaders_.clear();
    applyGraphicsShadersLength();
}

void System::removeGraphicsShaderAt(int index) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (index < 0 || static_cast<std::size_t>(index) >=
                         framePipeline.graphicsShaders_.size()) {
        return;
    }
    framePipeline.graphicsShaders_.erase(
        framePipeline.graphicsShaders_.begin() +
        static_cast<std::ptrdiff_t>(index));
    applyGraphicsShadersLength();
}

void System::flashScreen(std::optional<sf::Color> color, float duration) {
    if (duration <= 0.0f) {
        stopFlash();
        return;
    }
    if (!shadersAvailable()) {
        warnOnce("System.flashScreen",
                 "Shaders are unavailable; skipped screen flash effect");
        return;
    }
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (framePipeline.flashShader_ == nullptr) {
        try {
            framePipeline.flashShader_ =
                ShaderManager::load("Global/Flash.frag");
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
        addGraphicsShader(framePipeline.flashShader_);
        framePipeline.flashActive_ = true;
    }
    framePipeline.flashShader_->setUniform("flashColor",
                                           framePipeline.flashColour_);
    framePipeline.flashShader_->setUniform("intensity", 1.0f);
}

void System::stopFlash() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (framePipeline.flashActive_ && framePipeline.flashShader_ != nullptr) {
        removeGraphicsShader(framePipeline.flashShader_);
    }
    framePipeline.flashActive_ = false;
    framePipeline.flashTimeCount_ = 0.0f;
    framePipeline.flashDuration_ = 0.0f;
}

bool System::isFlashing() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return framePipeline.flashActive_;
}

void System::changeScreenTone(float red, float green, float blue, float gray,
                              float duration) {
    if (!shadersAvailable()) {
        warnOnce("System.changeScreenTone",
                 "Shaders are unavailable; skipped screen tone effect");
        return;
    }
    if (!ensureToneShader()) {
        return;
    }
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    const sf::Glsl::Vec4 target = makeToneColour(red, green, blue, gray);
    framePipeline.toneStartColour_ = framePipeline.toneCurrentColour_;
    framePipeline.toneTargetColour_ = target;
    framePipeline.toneDuration_ = std::max(0.0f, duration);
    framePipeline.toneTimeCount_ = 0.0f;
    framePipeline.toneActive_ = true;
    if (framePipeline.toneDuration_ <= 0.0f) {
        framePipeline.toneCurrentColour_ = target;
        applyScreenToneUniform();
        if (isNeutralTone(target)) {
            stopScreenTone();
        }
    } else {
        applyScreenToneUniform();
    }
}

void System::clearScreenTone(float duration) {
    changeScreenTone(0.0f, 0.0f, 0.0f, 0.0f, duration);
}

void System::stopScreenTone() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.toneCurrentColour_ = {};
    framePipeline.toneStartColour_ = {};
    framePipeline.toneTargetColour_ = {};
    framePipeline.toneDuration_ = 0.0f;
    framePipeline.toneTimeCount_ = 0.0f;
    framePipeline.toneActive_ = false;
}

bool System::isScreenToneActive() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return framePipeline.toneActive_;
}

bool System::isScreenToneTransitionComplete() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return !framePipeline.toneActive_ || framePipeline.toneDuration_ <= 0.0f;
}

void System::startShake(float power, float speed, float duration) {
    if (duration <= 0.0f) {
        stopShake();
        return;
    }
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.shakePower_ = power;
    framePipeline.shakeSpeed_ = speed;
    framePipeline.shakeDuration_ = duration;
    framePipeline.shakeTimeCount_ = 0.0f;
    framePipeline.shakeActive_ = true;
    framePipeline.shakeNextUpdate_ = 0.0f;
    framePipeline.shakeOffset_ = {};
}

void System::stopShake() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.shakeActive_ = false;
    framePipeline.shakeTimeCount_ = 0.0f;
    framePipeline.shakeDuration_ = 0.0f;
    framePipeline.shakeOffset_ = {};
}

bool System::isShaking() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return framePipeline.shakeActive_;
}

void System::cacheTransitionBackground() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (framePipeline.transition_ == nullptr ||
        !framePipeline.transitionOutputSprite_.has_value()) {
        return;
    }
    framePipeline.transition_->clear(sf::Color::Transparent);
    framePipeline.transition_->draw(*framePipeline.transitionOutputSprite_,
                                    sf::BlendNone);
    framePipeline.transition_->display();
}

void System::setTransition(
    const std::shared_ptr<sf::Texture>& transitionResource,
    float transitionTime) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.presentMutex_);
    ++framePipeline.transitionRevision_;
    framePipeline.transitionResource_ = transitionResource;
    if (framePipeline.transitionFreezePending_) {
        cacheTransitionBackground();
        framePipeline.transitionFreezePending_ = false;
        framePipeline.transitionFrozen_ = true;
    }
    if (framePipeline.transitionResource_ != nullptr &&
        framePipeline.transitionMaskTexture_ != nullptr) {
        const sf::Vector2u sourceSize =
            framePipeline.transitionResource_->getSize();
        const sf::Vector2u targetSize =
            framePipeline.transitionMaskTexture_->getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0 && targetSize.x > 0 &&
            targetSize.y > 0) {
            sf::Sprite maskSprite(*framePipeline.transitionResource_);
            maskSprite.setScale({static_cast<float>(targetSize.x) /
                                     static_cast<float>(sourceSize.x),
                                 static_cast<float>(targetSize.y) /
                                     static_cast<float>(sourceSize.y)});
            framePipeline.transitionMaskTexture_->clear(sf::Color::Transparent);
            framePipeline.transitionMaskTexture_->draw(maskSprite,
                                                       sf::BlendNone);
            framePipeline.transitionMaskTexture_->display();
        }
    }
    if (!framePipeline.transitionFrozen_) {
        cacheTransitionBackground();
    } else {
        framePipeline.transitionFrozen_ = false;
    }
    if (framePipeline.transitionShader_ == nullptr) {
        framePipeline.inTransition_ = false;
        return;
    }
    framePipeline.inTransition_ = true;
    framePipeline.transitionTimeCount_ = 0.0f;
    framePipeline.transitionTime_ = std::max(0.0f, transitionTime);
    if (framePipeline.transitionTempTexture_ != nullptr) {
        framePipeline.transitionTempTexture_->clear(sf::Color::Transparent);
    }
}

void System::freezeTransitionBackground() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.transitionFreezePending_ = true;
}

bool System::isTransitionBackgroundFrozen() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return framePipeline.transitionFrozen_;
}

bool System::isTransitionBackgroundFreezePending() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return framePipeline.transitionFreezePending_;
}

void System::cancelTransitionBackgroundFreeze() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.transitionFreezePending_ = false;
    framePipeline.transitionFrozen_ = false;
}

void System::requestTransition(std::optional<std::string> transitionName,
                               float transitionTime) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
    framePipeline.pendingTransition_ =
        PendingTransition{std::move(transitionName), transitionTime};
}

void System::cancelPendingTransition() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
    framePipeline.pendingTransition_.reset();
}

bool System::isTransitionPending() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
    return framePipeline.pendingTransition_.has_value();
}

bool System::isInTransition() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return framePipeline.inTransition_;
}

void System::applyPendingTransition() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    std::optional<PendingTransition> pending;
    {
        const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
        pending.swap(framePipeline.pendingTransition_);
    }
    if (!pending.has_value()) {
        return;
    }
    std::shared_ptr<sf::Texture> resource;
    if (pending->name.has_value() && !pending->name->empty()) {
        resource =
            TextureManager::load(ludork::global::manager::textureAssetFile(
                "Transitions", *pending->name));
    }
    setTransition(resource, pending->time);
}
void System::updateFlash(float deltaTime) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
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
        removeGraphicsShader(framePipeline.flashShader_);
        framePipeline.flashActive_ = false;
    }
}

void System::updateScreenTone(float deltaTime) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (!framePipeline.toneActive_ || framePipeline.toneShader_ == nullptr) {
        return;
    }
    if (framePipeline.toneDuration_ > 0.0f) {
        framePipeline.toneTimeCount_ =
            std::min(framePipeline.toneTimeCount_ + deltaTime,
                     framePipeline.toneDuration_);
        const float ratio = std::min(
            1.0f, framePipeline.toneTimeCount_ / framePipeline.toneDuration_);
        framePipeline.toneCurrentColour_ = {
            framePipeline.toneStartColour_.x +
                (framePipeline.toneTargetColour_.x -
                 framePipeline.toneStartColour_.x) *
                    ratio,
            framePipeline.toneStartColour_.y +
                (framePipeline.toneTargetColour_.y -
                 framePipeline.toneStartColour_.y) *
                    ratio,
            framePipeline.toneStartColour_.z +
                (framePipeline.toneTargetColour_.z -
                 framePipeline.toneStartColour_.z) *
                    ratio,
            framePipeline.toneStartColour_.w +
                (framePipeline.toneTargetColour_.w -
                 framePipeline.toneStartColour_.w) *
                    ratio};
    }
    applyScreenToneUniform();
    if (framePipeline.toneDuration_ > 0.0f &&
        framePipeline.toneTimeCount_ >= framePipeline.toneDuration_) {
        framePipeline.toneDuration_ = 0.0f;
        if (isNeutralTone(framePipeline.toneCurrentColour_)) {
            stopScreenTone();
        }
    }
}

void System::updateShake(float deltaTime) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (!framePipeline.shakeActive_) {
        return;
    }
    framePipeline.shakeTimeCount_ =
        std::min(framePipeline.shakeTimeCount_ + deltaTime,
                 framePipeline.shakeDuration_);
    if (framePipeline.shakeTimeCount_ >= framePipeline.shakeDuration_) {
        stopShake();
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

bool System::ensureToneShader() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (framePipeline.toneShader_ != nullptr) {
        return true;
    }
    try {
        framePipeline.toneShader_ = ShaderManager::load("Global/Tone.frag");
    } catch (const std::exception&) {
        framePipeline.toneShader_.reset();
        std::cerr << "TONE_SHADER_LOAD_FAILED\n";
        return false;
    }
    return framePipeline.toneShader_ != nullptr;
}

void System::applyScreenToneUniform() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (framePipeline.toneShader_ != nullptr) {
        framePipeline.toneShader_->setUniform("toneColor",
                                              framePipeline.toneCurrentColour_);
    }
}

void System::ensureToneBuffer(const sf::Vector2u& size) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
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

sf::Glsl::Vec4 System::makeToneColour(float red, float green, float blue,
                                      float gray) {
    return {std::clamp(red, -255.0f, 255.0f) / 255.0f,
            std::clamp(green, -255.0f, 255.0f) / 255.0f,
            std::clamp(blue, -255.0f, 255.0f) / 255.0f,
            std::clamp(gray, 0.0f, 255.0f) / 255.0f};
}

bool System::isNeutralTone(const sf::Glsl::Vec4& toneColour) {
    return std::abs(toneColour.x) <= 0.0001f &&
           std::abs(toneColour.y) <= 0.0001f &&
           std::abs(toneColour.z) <= 0.0001f &&
           std::abs(toneColour.w) <= 0.0001f;
}

void System::applyGraphicsShadersLength() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    while (framePipeline.graphicsCanvases_.size() >
           framePipeline.graphicsShaders_.size()) {
        framePipeline.graphicsCanvases_.pop_back();
    }
    if (display.canvas_ == nullptr) {
        return;
    }
    const sf::Vector2u size = display.canvas_->getSize();
    while (framePipeline.graphicsCanvases_.size() <
           framePipeline.graphicsShaders_.size()) {
        framePipeline.graphicsCanvases_.push_back(
            std::make_unique<sf::RenderTexture>(size));
    }
    for (std::unique_ptr<sf::RenderTexture>& graphicsCanvas :
         framePipeline.graphicsCanvases_) {
        if (graphicsCanvas->getSize() != size) {
            graphicsCanvas = std::make_unique<sf::RenderTexture>(size);
        }
    }
}

void System::setShaderUniform(sf::Shader& shader, const std::string& name,
                              const ShaderUniformValue& value) {
    std::visit(
        [&shader, &name](const auto& current) {
            using Value = std::decay_t<decltype(current)>;
            if constexpr (std::is_same_v<Value, sf::Color>) {
                shader.setUniform(name, sf::Glsl::Vec4(current));
            } else if constexpr (std::is_same_v<Value,
                                                std::shared_ptr<sf::Texture>>) {
                if (current != nullptr) {
                    shader.setUniform(name, *current);
                }
            } else if constexpr (std::is_same_v<Value, std::vector<float>> ||
                                 std::is_same_v<Value,
                                                std::vector<sf::Vector2f>> ||
                                 std::is_same_v<Value,
                                                std::vector<sf::Vector3f>> ||
                                 std::is_same_v<Value,
                                                std::vector<sf::Glsl::Vec4>>) {
                if (!current.empty()) {
                    shader.setUniformArray(name, current.data(),
                                           current.size());
                }
            } else {
                shader.setUniform(name, current);
            }
        },
        value);
}

bool System::shadersAvailable() {
    return sf::Shader::isAvailable();
}
