#include "FramePipelineImpl.hpp"
#include "DisplayImpl.hpp"
#include <EngineState.hpp>
#include <Fog/FogController.hpp>
#include <Manager/ShaderManager.hpp>
#include <Utils/Inner.hpp>
#include <Utils/Render.hpp>
#include <Weather/WeatherController.hpp>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace ludork::global::system_impl {

void FramePipelineImpl::initCanvas(const sf::Vector2u& size) {
    std::optional<sf::View> preservedView;
    if (canvas_ == nullptr) {
        canvas_ = std::make_unique<sf::RenderTexture>(size);
    } else {
        const sf::View currentView = canvas_->getView();
        if (!canvasDefaultViewActive_ ||
            !viewsEqual(currentView, canvas_->getDefaultView())) {
            preservedView = currentView;
        }
        if (canvas_->getSize() != size && !canvas_->resize(size)) {
            throw std::runtime_error("Failed to resize the System canvas");
        }
    }
    canvas_->setView(canvas_->getDefaultView());
    canvas_->clear(sf::Color::Transparent);
    canvas_->setView(preservedView.value_or(canvas_->getDefaultView()));
    if (canvasSprite_.has_value()) {
        canvasSprite_->setTexture(canvas_->getTexture(), true);
    } else {
        canvasSprite_.emplace(canvas_->getTexture());
    }
    transition_ = std::make_unique<sf::RenderTexture>(size);
    transition_->clear(sf::Color::Transparent);
    transition_->display();
    transitionTempTexture_ = std::make_unique<sf::RenderTexture>(size);
    transitionTempTexture_->clear(sf::Color::Transparent);
    transitionOutputTexture_ = std::make_unique<sf::RenderTexture>(size);
    transitionOutputTexture_->clear(sf::Color::Transparent);
    transitionOutputTexture_->display();
    transitionMaskTexture_ = std::make_unique<sf::RenderTexture>(size);
    transitionMaskTexture_->clear(sf::Color::Transparent);
    transitionMaskTexture_->display();
    transitionSprite_.emplace(transitionTempTexture_->getTexture());
    transitionOutputSprite_.emplace(transitionOutputTexture_->getTexture());
    toneBuffer_.reset();
    toneBufferSprite_.reset();
    applyGraphicsShadersLength();
}

void FramePipelineImpl::draw(const sf::Drawable& drawable, sf::Shader* shader) {
    if (canvas_ == nullptr) {
        return;
    }
    sf::RenderStates states = canvasRenderStates();
    states.shader = shader;
    canvas_->draw(drawable, states);
}

void FramePipelineImpl::composeFrame(float deltaTime,
                                     sf::RenderTarget* target) {
    transitionCompletionPending_ = false;
    if (target == nullptr || canvas_ == nullptr || !canvasSprite_.has_value()) {
        return;
    }
    if (inTransition_) {
        transitionTimeCount_ =
            advanceElapsed(transitionTimeCount_, transitionTime_, deltaTime);
    }
    updateFlash(deltaTime);
    updateScreenTone(deltaTime);
    updateShake(deltaTime);
    WeatherController::update(deltaTime);
    FogController::update(deltaTime);
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
        target->draw(*canvasSprite_, canvasRenderStates());
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
        target->draw(*transitionOutputSprite_, canvasRenderStates());
    } else {
        transitionOutputTexture_->clear(sf::Color::Transparent);
        transitionOutputTexture_->draw(*canvasSprite_, sf::BlendNone);
        transitionOutputTexture_->display();
        target->draw(*transitionOutputSprite_, canvasRenderStates());
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
        inTransition_ && isComplete(transitionTimeCount_, transitionTime_);
}

void FramePipelineImpl::present(DisplayImpl& display) {
    const std::lock_guard<std::mutex> lock(presentMutex_);
    if (canvas_ == nullptr || !canvasSprite_.has_value()) {
        return;
    }
    display.present();
}

bool FramePipelineImpl::completeFrame(bool hasWindow) {
    if (!hasWindow || canvas_ == nullptr || !canvasSprite_.has_value()) {
        transitionCompletionPending_ = false;
        return false;
    }
    if (transitionCompletionPending_ &&
        composedTransitionRevision_ == transitionRevision_) {
        inTransition_ = false;
    }
    transitionCompletionPending_ = false;
    return true;
}

void FramePipelineImpl::addGraphicsShader(
    const std::shared_ptr<sf::Shader>& shader,
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

void FramePipelineImpl::removeGraphicsShader(
    const std::shared_ptr<sf::Shader>& shader) {
    const auto iterator =
        std::find(graphicsShaders_.begin(), graphicsShaders_.end(), shader);
    if (iterator != graphicsShaders_.end()) {
        graphicsShaders_.erase(iterator);
    }
    applyGraphicsShadersLength();
}

void FramePipelineImpl::removeAllGraphicsShaders() {
    graphicsShaders_.clear();
    applyGraphicsShadersLength();
}

void FramePipelineImpl::removeGraphicsShaderAt(int index) {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= graphicsShaders_.size()) {
        return;
    }
    graphicsShaders_.erase(graphicsShaders_.begin() +
                           static_cast<std::ptrdiff_t>(index));
    applyGraphicsShadersLength();
}

void FramePipelineImpl::applyGraphicsShadersLength() {
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

void FramePipelineImpl::setShaderUniform(sf::Shader& shader,
                                         const std::string& name,
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

bool FramePipelineImpl::shadersAvailable() {
    return sf::Shader::isAvailable();
}

bool FramePipelineImpl::viewsEqual(const sf::View& left,
                                   const sf::View& right) {
    return left.getCenter() == right.getCenter() &&
           left.getSize() == right.getSize() &&
           left.getRotation() == right.getRotation() &&
           left.getViewport() == right.getViewport() &&
           left.getScissor() == right.getScissor();
}

void FramePipelineImpl::setWindowMapView(const sf::IntRect& rect) {
    if (canvas_ == nullptr) {
        return;
    }
    const sf::Vector2u gameSize = engineState().getGameSize();
    const sf::Vector2f gameSizeFloat{static_cast<float>(gameSize.x),
                                     static_cast<float>(gameSize.y)};
    const sf::Vector2f position{static_cast<float>(rect.position.x),
                                static_cast<float>(rect.position.y)};
    const sf::Vector2f size{static_cast<float>(rect.size.x),
                            static_cast<float>(rect.size.y)};
    sf::View view(size / 2.0f, size);
    view.setViewport(sf::FloatRect(position.componentWiseDiv(gameSizeFloat),
                                   size.componentWiseDiv(gameSizeFloat)));
    canvas_->setView(view);
    canvasDefaultViewActive_ = false;
}

void FramePipelineImpl::setWindowDefaultView() {
    if (canvas_ != nullptr) {
        canvas_->setView(canvas_->getDefaultView());
        canvasDefaultViewActive_ = true;
    }
}

sf::RenderTexture* FramePipelineImpl::getCanvas() {
    return canvas_.get();
}

void FramePipelineImpl::clearCanvas() {
    if (canvas_ != nullptr) {
        canvas_->clear(sf::Color::Transparent);
    }
}

sf::Vector2u FramePipelineImpl::getCanvasSize() const {
    return canvas_ != nullptr ? canvas_->getSize() : sf::Vector2u{};
}

void FramePipelineImpl::initializeGraphics() {
    if (shadersAvailable()) {
        transitionShader_ =
            ShaderManager::load("/Game/Assets/Shaders/Global/Transition.frag",
                                sf::Shader::Type::Fragment);
    } else {
        transitionShader_.reset();
        warnOnce("System.transitionShader",
                 "Shaders are unavailable; skipped loading transition shader");
    }
}

void FramePipelineImpl::rebuildTargets(const sf::Vector2u& size,
                                       float renderScale) {
    std::optional<sf::Image> transitionImage;
    if (transition_ != nullptr) {
        transition_->display();
        transitionImage = transition_->getTexture().copyToImage();
    }
    engineState().setScale(renderScale);
    initCanvas(size);
    if (transitionImage.has_value() && transition_ != nullptr) {
        const sf::Texture texture(*transitionImage);
        sf::Sprite sprite(texture);
        const sf::Vector2u sourceSize = texture.getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0) {
            sprite.setScale(
                {static_cast<float>(size.x) / static_cast<float>(sourceSize.x),
                 static_cast<float>(size.y) /
                     static_cast<float>(sourceSize.y)});
            transition_->clear(sf::Color::Transparent);
            transition_->draw(sprite, sf::BlendNone);
            transition_->display();
        }
    }
    if (transitionResource_ != nullptr && transitionMaskTexture_ != nullptr) {
        const sf::Vector2u sourceSize = transitionResource_->getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0) {
            sf::Sprite maskSprite(*transitionResource_);
            maskSprite.setScale(
                {static_cast<float>(size.x) / static_cast<float>(sourceSize.x),
                 static_cast<float>(size.y) /
                     static_cast<float>(sourceSize.y)});
            transitionMaskTexture_->clear(sf::Color::Transparent);
            transitionMaskTexture_->draw(maskSprite, sf::BlendNone);
            transitionMaskTexture_->display();
        }
    }
}

void FramePipelineImpl::reset() {
    graphicsCanvases_.clear();
    graphicsShaders_.clear();
    {
        const std::lock_guard<std::mutex> lock(transitionMutex_);
        pendingTransition_.reset();
    }
    transitionResource_.reset();
    transitionFrozen_ = false;
    transitionFreezePending_ = false;
    inTransition_ = false;
    transitionTimeCount_ = 0.0f;
    transitionTime_ = 0.0f;
    transitionRevision_ = 0;
    composedTransitionRevision_ = 0;
    transitionCompletionPending_ = false;
    stopFlash();
    stopScreenTone();
    stopShake();
    transitionShader_.reset();
    canvasDefaultViewActive_ = true;
}

void FramePipelineImpl::shutdown() noexcept {
    {
        const std::lock_guard<std::mutex> lock(transitionMutex_);
        pendingTransition_.reset();
    }
    graphicsShaders_.clear();
    graphicsCanvases_.clear();
    transitionResource_.reset();
    transitionShader_.reset();
    flashShader_.reset();
    toneShader_.reset();
    canvasSprite_.reset();
    transitionSprite_.reset();
    transitionOutputSprite_.reset();
    toneBufferSprite_.reset();
    transition_.reset();
    transitionTempTexture_.reset();
    transitionOutputTexture_.reset();
    transitionMaskTexture_.reset();
    toneBuffer_.reset();
    canvas_.reset();
    inTransition_ = false;
    transitionTimeCount_ = 0.0f;
    transitionTime_ = 0.0f;
    transitionRevision_ = 0;
    composedTransitionRevision_ = 0;
    transitionCompletionPending_ = false;
    transitionFrozen_ = false;
    transitionFreezePending_ = false;
    flashActive_ = false;
    flashColour_ = {1.0f, 1.0f, 1.0f, 1.0f};
    flashDuration_ = 0.0f;
    flashTimeCount_ = 0.0f;
    toneActive_ = false;
    toneCurrentColour_ = {};
    toneStartColour_ = {};
    toneTargetColour_ = {};
    toneDuration_ = 0.0f;
    toneTimeCount_ = 0.0f;
    shakeActive_ = false;
    shakePower_ = 0.0f;
    shakeSpeed_ = 0.0f;
    shakeDuration_ = 0.0f;
    shakeTimeCount_ = 0.0f;
    shakeOffset_ = {};
    shakeNextUpdate_ = 0.0f;
    canvasDefaultViewActive_ = true;
}

}  // namespace ludork::global::system_impl
