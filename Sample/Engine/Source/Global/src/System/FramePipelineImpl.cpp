#include "FramePipelineImpl.hpp"
#include "SystemImpl.hpp"
#include "DisplayImpl.hpp"
#include "ScreenEffectsImpl.hpp"
#include "TransitionImpl.hpp"
#include "Platform/NativeInputMethod.hpp"
#include "Platform/NativeDisplay.hpp"
#include "Diagnostics/PerformanceProfiler.hpp"
#include <Fog/FogController.hpp>
#include <Utils/Inner.hpp>
#include <Utils/Render.hpp>
#include <Weather/WeatherController.hpp>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <type_traits>

namespace ludork::global::system_frame_pipeline_impl {

void initCanvas(const sf::Vector2u& size) {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::DisplayImpl& display = systemImpl.display;
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        systemImpl.framePipeline;
    std::optional<sf::View> preservedView;
    if (display.canvas_ == nullptr) {
        display.canvas_ = std::make_unique<sf::RenderTexture>(size);
    } else {
        const sf::View currentView = display.canvas_->getView();
        if (!display.canvasDefaultViewActive_ ||
            !ludork::global::system_display_impl::viewsEqual(
                currentView, display.canvas_->getDefaultView())) {
            preservedView = currentView;
        }
        if (display.canvas_->getSize() != size &&
            !display.canvas_->resize(size)) {
            throw std::runtime_error("Failed to resize the System canvas");
        }
    }
    display.canvas_->setView(display.canvas_->getDefaultView());
    display.canvas_->clear(sf::Color::Transparent);
    display.canvas_->setView(
        preservedView.value_or(display.canvas_->getDefaultView()));
    if (display.canvasSprite_.has_value()) {
        display.canvasSprite_->setTexture(display.canvas_->getTexture(), true);
    } else {
        display.canvasSprite_.emplace(display.canvas_->getTexture());
    }
    framePipeline.transition_ = std::make_unique<sf::RenderTexture>(size);
    framePipeline.transition_->clear(sf::Color::Transparent);
    framePipeline.transition_->display();
    framePipeline.transitionTempTexture_ =
        std::make_unique<sf::RenderTexture>(size);
    framePipeline.transitionTempTexture_->clear(sf::Color::Transparent);
    framePipeline.transitionOutputTexture_ =
        std::make_unique<sf::RenderTexture>(size);
    framePipeline.transitionOutputTexture_->clear(sf::Color::Transparent);
    framePipeline.transitionOutputTexture_->display();
    framePipeline.transitionMaskTexture_ =
        std::make_unique<sf::RenderTexture>(size);
    framePipeline.transitionMaskTexture_->clear(sf::Color::Transparent);
    framePipeline.transitionMaskTexture_->display();
    framePipeline.transitionSprite_.emplace(
        framePipeline.transitionTempTexture_->getTexture());
    framePipeline.transitionOutputSprite_.emplace(
        framePipeline.transitionOutputTexture_->getTexture());
    framePipeline.toneBuffer_.reset();
    framePipeline.toneBufferSprite_.reset();
    ludork::global::system_frame_pipeline_impl::applyGraphicsShadersLength();
}

void draw(const sf::Drawable& drawable, sf::Shader* shader) {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    if (display.canvas_ == nullptr) {
        return;
    }
    sf::RenderStates states = canvasRenderStates();
    states.shader = shader;
    display.canvas_->draw(drawable, states);
}

void composeFrame(float deltaTime) {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::DisplayImpl& display = systemImpl.display;
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        systemImpl.framePipeline;
    framePipeline.transitionCompletionPending_ = false;
    if (display.window_ == nullptr || display.canvas_ == nullptr ||
        !display.canvasSprite_.has_value()) {
        return;
    }
    if (framePipeline.inTransition_) {
        framePipeline.transitionTimeCount_ =
            ludork::global::system_transition_impl::advanceElapsed(
                framePipeline.transitionTimeCount_,
                framePipeline.transitionTime_, deltaTime);
    }
    ludork::global::system_screen_effects_impl::updateFlash(deltaTime);
    ludork::global::system_screen_effects_impl::updateScreenTone(deltaTime);
    ludork::global::system_screen_effects_impl::updateShake(deltaTime);
    WeatherController::update(deltaTime);
    FogController::update(deltaTime);
    ludork::global::system_transition_impl::applyPendingTransition();
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
        ludork::global::system_transition_impl::cacheTransitionBackground();
        framePipeline.transitionFreezePending_ = false;
        framePipeline.transitionFrozen_ = true;
    }
    framePipeline.composedTransitionRevision_ =
        framePipeline.transitionRevision_;
    framePipeline.transitionCompletionPending_ =
        framePipeline.inTransition_ &&
        ludork::global::system_transition_impl::isComplete(
            framePipeline.transitionTimeCount_, framePipeline.transitionTime_);
}

void present() {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::DisplayImpl& display = systemImpl.display;
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        systemImpl.framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.presentMutex_);
    if (display.window_ == nullptr || display.canvas_ == nullptr ||
        !display.canvasSprite_.has_value()) {
        return;
    }
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
    if (!ludork::global::system_display_impl::isEmbeddedDisplay() &&
        !display.desktopFullscreen_) {
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
                    ludork::global::system_display_impl::windowFitScale(
                        clientSize.value_or(windowSize));
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

void completeFrame() {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::DisplayImpl& display = systemImpl.display;
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        systemImpl.framePipeline;
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
    ludork::global::system_display_impl::applyPendingDisplayChanges();
}

void addGraphicsShader(const std::shared_ptr<sf::Shader>& shader,
                       std::optional<ShaderUniforms> uniforms) {
    if (!ludork::global::system_frame_pipeline_impl::shadersAvailable()) {
        if (shader != nullptr) {
            warnOnce("System.addGraphicsShader",
                     "Shaders are unavailable; ignored addGraphicsShader");
        }
        return;
    }
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    framePipeline.graphicsShaders_.push_back(shader);
    if (shader != nullptr && uniforms.has_value()) {
        for (const auto& [name, value] : *uniforms) {
            ludork::global::system_frame_pipeline_impl::setShaderUniform(
                *shader, name, value);
        }
    }
    ludork::global::system_frame_pipeline_impl::applyGraphicsShadersLength();
}

void removeGraphicsShader(const std::shared_ptr<sf::Shader>& shader) {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    const auto iterator =
        std::find(framePipeline.graphicsShaders_.begin(),
                  framePipeline.graphicsShaders_.end(), shader);
    if (iterator != framePipeline.graphicsShaders_.end()) {
        framePipeline.graphicsShaders_.erase(iterator);
    }
    ludork::global::system_frame_pipeline_impl::applyGraphicsShadersLength();
}

void removeAllGraphicsShaders() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    framePipeline.graphicsShaders_.clear();
    ludork::global::system_frame_pipeline_impl::applyGraphicsShadersLength();
}

void removeGraphicsShaderAt(int index) {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    if (index < 0 || static_cast<std::size_t>(index) >=
                         framePipeline.graphicsShaders_.size()) {
        return;
    }
    framePipeline.graphicsShaders_.erase(
        framePipeline.graphicsShaders_.begin() +
        static_cast<std::ptrdiff_t>(index));
    ludork::global::system_frame_pipeline_impl::applyGraphicsShadersLength();
}

void applyGraphicsShadersLength() {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::DisplayImpl& display = systemImpl.display;
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        systemImpl.framePipeline;
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

void setShaderUniform(sf::Shader& shader, const std::string& name,
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

bool shadersAvailable() {
    return sf::Shader::isAvailable();
}

}  // namespace ludork::global::system_frame_pipeline_impl
