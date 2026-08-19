#include <System.hpp>

#include "Diagnostics/PerformanceProfiler.hpp"
#include "Platform/NativeDisplay.hpp"
#include "Platform/NativeInputMethod.hpp"
#include "SystemRuntimeAccess.hpp"

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

using namespace ludork::global::system_runtime;

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
    if (canvas_ == nullptr) {
        return;
    }
    sf::RenderStates states = canvasRenderStates();
    states.shader = shader;
    canvas_->draw(drawable, states);
}

void System::applyScreenTonePass() {
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

void System::composeFrame(float deltaTime) {
    transitionCompletionPending_ = false;
    if (window_ == nullptr || canvas_ == nullptr ||
        !canvasSprite_.has_value()) {
        return;
    }
    if (inTransition_) {
        transitionTimeCount_ =
            std::min(transitionTimeCount_ + deltaTime, transitionTime_);
    }
    updateFlash(deltaTime);
    updateScreenTone(deltaTime);
    updateShake(deltaTime);
    updateWeather(deltaTime);
    updateFog(deltaTime);
    applyPendingTransition();
    canvas_->display();
    sf::RenderTexture* finalCanvas = canvas_.get();
    for (std::size_t index = 0; index < graphicsCanvases_.size(); ++index) {
        sf::RenderTexture& target = *graphicsCanvases_[index];
        sf::RenderTexture& source =
            index == 0 ? *canvas_ : *graphicsCanvases_[index - 1];
        target.clear(sf::Color::Transparent);
        sf::Sprite sprite(source.getTexture());
        sf::RenderStates states = canvasRenderStates();
        const std::shared_ptr<sf::Shader>& shader = graphicsShaders_[index];
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
    canvasSprite_->setTexture(finalCanvas->getTexture(), true);
    if (shakeActive_) {
        const sf::Vector2u textureSize = finalCanvas->getSize();
        if (textureSize.x > 0 && textureSize.y > 0) {
            const float pad = shakePower_;
            canvasSprite_->setScale(
                {(static_cast<float>(textureSize.x) + pad * 2.0f) /
                     static_cast<float>(textureSize.x),
                 (static_cast<float>(textureSize.y) + pad * 2.0f) /
                     static_cast<float>(textureSize.y)});
            canvasSprite_->setPosition(
                {-pad + shakeOffset_.x, -pad + shakeOffset_.y});
        }
    }
    if (transitionOutputTexture_ == nullptr ||
        !transitionOutputSprite_.has_value()) {
        window_->draw(*canvasSprite_, canvasRenderStates());
    } else if (inTransition_ && transitionShader_ != nullptr &&
               transition_ != nullptr && transitionTempTexture_ != nullptr &&
               transitionSprite_.has_value()) {
        transitionTempTexture_->clear(sf::Color::Transparent);
        transitionTempTexture_->draw(*canvasSprite_, sf::BlendNone);
        transitionTempTexture_->display();
        transitionShader_->setUniform("screenTex",
                                      transitionTempTexture_->getTexture());
        transitionShader_->setUniform("backTex", transition_->getTexture());
        transitionShader_->setUniform(
            "transitionResource",
            transitionResource_ != nullptr && transitionMaskTexture_ != nullptr
                ? transitionMaskTexture_->getTexture()
                : transition_->getTexture());
        transitionShader_->setUniform("useMask",
                                      transitionResource_ != nullptr &&
                                          transitionMaskTexture_ != nullptr);
        transitionShader_->setUniform("progress", transitionTimeCount_);
        transitionShader_->setUniform("totalTime", transitionTime_);
        sf::RenderStates states(sf::BlendNone);
        states.shader = transitionShader_.get();
        transitionOutputTexture_->clear(sf::Color::Transparent);
        transitionOutputTexture_->draw(*transitionSprite_, states);
        transitionOutputTexture_->display();
        window_->draw(*transitionOutputSprite_, canvasRenderStates());
    } else {
        transitionOutputTexture_->clear(sf::Color::Transparent);
        transitionOutputTexture_->draw(*canvasSprite_, sf::BlendNone);
        transitionOutputTexture_->display();
        window_->draw(*transitionOutputSprite_, canvasRenderStates());
    }
    if (shakeActive_) {
        canvasSprite_->setScale({1.0f, 1.0f});
        canvasSprite_->setPosition({0.0f, 0.0f});
    }
    if (transitionFreezePending_) {
        cacheTransitionBackground();
        transitionFreezePending_ = false;
        transitionFrozen_ = true;
    }
    composedTransitionRevision_ = transitionRevision_;
    transitionCompletionPending_ =
        inTransition_ && transitionTimeCount_ >= transitionTime_;
}

void System::present() {
    const std::lock_guard<std::mutex> lock(presentMutex_);
    if (window_ == nullptr || canvas_ == nullptr ||
        !canvasSprite_.has_value()) {
        return;
    }
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
    if (!isEmbeddedDisplay() && !desktopFullscreen_) {
        const sf::Vector2u windowSize = window_->getSize();
        const bool liveResizing = ludork::global::isNativeWindowLiveResizing(
            window_->getNativeHandle());
        if (liveResizing || windowSize != observedWindowSize_) {
            const std::optional<sf::Vector2u> clientSize =
                ludork::global::getWindowedClientSize(
                    window_->getNativeHandle());
            if (liveResizing || !clientSize.has_value() ||
                clientSize != observedWindowClientSize_) {
                pendingResizeScale_ =
                    windowFitScale(clientSize.value_or(windowSize));
                lastResizeTime_ = std::chrono::steady_clock::now();
            }
            return;
        }
        if (pendingResizeScale_.has_value()) {
            return;
        }
    }
#endif
    if (PerformanceProfiler::isEnabled()) {
        const auto presentStart = std::chrono::steady_clock::now();
        window_->display();
        PerformanceProfiler::addPresentWait(
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - presentStart)
                .count());
    } else {
        window_->display();
    }
}

void System::completeFrame() {
    if (window_ == nullptr || canvas_ == nullptr ||
        !canvasSprite_.has_value()) {
        transitionCompletionPending_ = false;
        return;
    }
    if (transitionCompletionPending_ &&
        composedTransitionRevision_ == transitionRevision_) {
        inTransition_ = false;
    }
    transitionCompletionPending_ = false;
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
    graphicsShaders_.push_back(shader);
    if (shader != nullptr && uniforms.has_value()) {
        for (const auto& [name, value] : *uniforms) {
            setShaderUniform(*shader, name, value);
        }
    }
    applyGraphicsShadersLength();
}

void System::removeGraphicsShader(const std::shared_ptr<sf::Shader>& shader) {
    const auto iterator =
        std::find(graphicsShaders_.begin(), graphicsShaders_.end(), shader);
    if (iterator != graphicsShaders_.end()) {
        graphicsShaders_.erase(iterator);
    }
    applyGraphicsShadersLength();
}

void System::removeAllGraphicsShaders() {
    graphicsShaders_.clear();
    applyGraphicsShadersLength();
}

void System::removeGraphicsShaderAt(int index) {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= graphicsShaders_.size()) {
        return;
    }
    graphicsShaders_.erase(graphicsShaders_.begin() +
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
    if (flashShader_ == nullptr) {
        try {
            flashShader_ = ShaderManager::load("Global/Flash.frag");
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

void System::stopFlash() {
    if (flashActive_ && flashShader_ != nullptr) {
        removeGraphicsShader(flashShader_);
    }
    flashActive_ = false;
    flashTimeCount_ = 0.0f;
    flashDuration_ = 0.0f;
}

bool System::isFlashing() {
    return flashActive_;
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

void System::clearScreenTone(float duration) {
    changeScreenTone(0.0f, 0.0f, 0.0f, 0.0f, duration);
}

void System::stopScreenTone() {
    toneCurrentColour_ = {};
    toneStartColour_ = {};
    toneTargetColour_ = {};
    toneDuration_ = 0.0f;
    toneTimeCount_ = 0.0f;
    toneActive_ = false;
}

bool System::isScreenToneActive() {
    return toneActive_;
}

bool System::isScreenToneTransitionComplete() {
    return !toneActive_ || toneDuration_ <= 0.0f;
}

void System::startShake(float power, float speed, float duration) {
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

void System::stopShake() {
    shakeActive_ = false;
    shakeTimeCount_ = 0.0f;
    shakeDuration_ = 0.0f;
    shakeOffset_ = {};
}

bool System::isShaking() {
    return shakeActive_;
}

void System::cacheTransitionBackground() {
    if (transition_ == nullptr || !transitionOutputSprite_.has_value()) {
        return;
    }
    transition_->clear(sf::Color::Transparent);
    transition_->draw(*transitionOutputSprite_, sf::BlendNone);
    transition_->display();
}

void System::setTransition(
    const std::shared_ptr<sf::Texture>& transitionResource,
    float transitionTime) {
    const std::lock_guard<std::mutex> lock(presentMutex_);
    ++transitionRevision_;
    transitionResource_ = transitionResource;
    if (transitionFreezePending_) {
        cacheTransitionBackground();
        transitionFreezePending_ = false;
        transitionFrozen_ = true;
    }
    if (transitionResource_ != nullptr && transitionMaskTexture_ != nullptr) {
        const sf::Vector2u sourceSize = transitionResource_->getSize();
        const sf::Vector2u targetSize = transitionMaskTexture_->getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0 && targetSize.x > 0 &&
            targetSize.y > 0) {
            sf::Sprite maskSprite(*transitionResource_);
            maskSprite.setScale({static_cast<float>(targetSize.x) /
                                     static_cast<float>(sourceSize.x),
                                 static_cast<float>(targetSize.y) /
                                     static_cast<float>(sourceSize.y)});
            transitionMaskTexture_->clear(sf::Color::Transparent);
            transitionMaskTexture_->draw(maskSprite, sf::BlendNone);
            transitionMaskTexture_->display();
        }
    }
    if (!transitionFrozen_) {
        cacheTransitionBackground();
    } else {
        transitionFrozen_ = false;
    }
    if (transitionShader_ == nullptr) {
        inTransition_ = false;
        return;
    }
    inTransition_ = true;
    transitionTimeCount_ = 0.0f;
    transitionTime_ = std::max(0.0f, transitionTime);
    if (transitionTempTexture_ != nullptr) {
        transitionTempTexture_->clear(sf::Color::Transparent);
    }
}

void System::freezeTransitionBackground() {
    transitionFreezePending_ = true;
}

bool System::isTransitionBackgroundFrozen() {
    return transitionFrozen_;
}

bool System::isTransitionBackgroundFreezePending() {
    return transitionFreezePending_;
}

void System::cancelTransitionBackgroundFreeze() {
    transitionFreezePending_ = false;
    transitionFrozen_ = false;
}

void System::requestTransition(std::optional<std::string> transitionName,
                               float transitionTime) {
    const std::lock_guard<std::mutex> lock(transitionMutex_);
    pendingTransition_ =
        PendingTransition{std::move(transitionName), transitionTime};
}

void System::cancelPendingTransition() {
    const std::lock_guard<std::mutex> lock(transitionMutex_);
    pendingTransition_.reset();
}

bool System::isTransitionPending() {
    const std::lock_guard<std::mutex> lock(transitionMutex_);
    return pendingTransition_.has_value();
}

bool System::isInTransition() {
    return inTransition_;
}

void System::applyPendingTransition() {
    std::optional<PendingTransition> pending;
    {
        const std::lock_guard<std::mutex> lock(transitionMutex_);
        pending.swap(pendingTransition_);
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

void System::updateScreenTone(float deltaTime) {
    if (!toneActive_ || toneShader_ == nullptr) {
        return;
    }
    if (toneDuration_ > 0.0f) {
        toneTimeCount_ = std::min(toneTimeCount_ + deltaTime, toneDuration_);
        const float ratio = std::min(1.0f, toneTimeCount_ / toneDuration_);
        toneCurrentColour_ = {
            toneStartColour_.x +
                (toneTargetColour_.x - toneStartColour_.x) * ratio,
            toneStartColour_.y +
                (toneTargetColour_.y - toneStartColour_.y) * ratio,
            toneStartColour_.z +
                (toneTargetColour_.z - toneStartColour_.z) * ratio,
            toneStartColour_.w +
                (toneTargetColour_.w - toneStartColour_.w) * ratio};
    }
    applyScreenToneUniform();
    if (toneDuration_ > 0.0f && toneTimeCount_ >= toneDuration_) {
        toneDuration_ = 0.0f;
        if (isNeutralTone(toneCurrentColour_)) {
            stopScreenTone();
        }
    }
}

void System::updateShake(float deltaTime) {
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

bool System::ensureToneShader() {
    if (toneShader_ != nullptr) {
        return true;
    }
    try {
        toneShader_ = ShaderManager::load("Global/Tone.frag");
    } catch (const std::exception&) {
        toneShader_.reset();
        std::cerr << "TONE_SHADER_LOAD_FAILED\n";
        return false;
    }
    return toneShader_ != nullptr;
}

void System::applyScreenToneUniform() {
    if (toneShader_ != nullptr) {
        toneShader_->setUniform("toneColor", toneCurrentColour_);
    }
}

void System::ensureToneBuffer(const sf::Vector2u& size) {
    if (toneBuffer_ == nullptr || toneBuffer_->getSize() != size) {
        toneBuffer_ = std::make_unique<sf::RenderTexture>(size);
        toneBufferSprite_.emplace(toneBuffer_->getTexture());
    } else if (!toneBufferSprite_.has_value()) {
        toneBufferSprite_.emplace(toneBuffer_->getTexture());
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
    while (graphicsCanvases_.size() > graphicsShaders_.size()) {
        graphicsCanvases_.pop_back();
    }
    if (canvas_ == nullptr) {
        return;
    }
    const sf::Vector2u size = canvas_->getSize();
    while (graphicsCanvases_.size() < graphicsShaders_.size()) {
        graphicsCanvases_.push_back(std::make_unique<sf::RenderTexture>(size));
    }
    for (std::unique_ptr<sf::RenderTexture>& graphicsCanvas :
         graphicsCanvases_) {
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
