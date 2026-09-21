#include "FramePipelineImpl.hpp"
#include "DisplayImpl.hpp"
#include <EngineState.hpp>
#include <Fog/FogController.hpp>
#include <Utils/Inner.hpp>
#include <Utils/Render.hpp>
#include <Weather/WeatherController.hpp>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace ludork::global::system_impl {

FramePipelineImpl& framePipelineImpl() {
    static FramePipelineImpl instance;
    return instance;
}

TransitionImpl& transitionImpl() {
    return framePipelineImpl().transition();
}

FramePipelineImpl::FramePipelineImpl()
    : screenEffectsImpl_(*this), transitionImpl_(presentMutex_) {}

ScreenEffectsImpl& FramePipelineImpl::screenEffects() {
    return screenEffectsImpl_;
}

TransitionImpl& FramePipelineImpl::transition() {
    return transitionImpl_;
}

void FramePipelineImpl::addEffectShader(
    const std::shared_ptr<sf::Shader>& shader) {
    addGraphicsShader(shader);
}

void FramePipelineImpl::removeEffectShader(
    const std::shared_ptr<sf::Shader>& shader) {
    removeGraphicsShader(shader);
}

void FramePipelineImpl::initCanvas(const sf::Vector2u& size,
                                   bool preserveTransitionBackground) {
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
            throw std::runtime_error("Failed to resize the Graphics canvas");
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
    if (preserveTransitionBackground) {
        transitionImpl_.rebuildTargets(size);
    } else {
        transitionImpl_.initializeTargets(size);
    }
    screenEffectsImpl_.invalidateTargets();
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
    transitionImpl_.beginFrame();
    if (target == nullptr || canvas_ == nullptr || !canvasSprite_.has_value()) {
        return;
    }
    transitionImpl_.advance(deltaTime);
    screenEffectsImpl_.update(deltaTime);
    WeatherController::update(deltaTime);
    FogController::update(deltaTime);
    transitionImpl_.applyPendingTransition();
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
    screenEffectsImpl_.applyShake(*canvasSprite_, finalCanvas->getSize());
    transitionImpl_.compose(*canvasSprite_, *target);
    screenEffectsImpl_.restoreShake(*canvasSprite_);
    transitionImpl_.finishComposition();
}

void FramePipelineImpl::present(DisplayImpl& display) {
    const std::lock_guard<std::mutex> lock(presentMutex_);
    if (canvas_ == nullptr || !canvasSprite_.has_value()) {
        return;
    }
    display.present();
}

bool FramePipelineImpl::completeFrame(bool hasWindow) {
    const bool submitted =
        hasWindow && canvas_ != nullptr && canvasSprite_.has_value();
    transitionImpl_.completeFrame(submitted);
    return submitted;
}

void FramePipelineImpl::addGraphicsShader(
    const std::shared_ptr<sf::Shader>& shader,
    std::optional<ShaderUniforms> uniforms) {
    if (!shadersAvailable()) {
        if (shader != nullptr) {
            warnOnce("Graphics.addGraphicsShader",
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
    transitionImpl_.initializeGraphics();
}

void FramePipelineImpl::rebuildTargets(const sf::Vector2u& size,
                                       float renderScale) {
    engineState().setScale(renderScale);
    initCanvas(size, true);
}

void FramePipelineImpl::reset() {
    graphicsCanvases_.clear();
    graphicsShaders_.clear();
    transitionImpl_.reset();
    screenEffectsImpl_.reset();
    canvasDefaultViewActive_ = true;
}

void FramePipelineImpl::shutdown() noexcept {
    graphicsShaders_.clear();
    graphicsCanvases_.clear();
    transitionImpl_.shutdown();
    screenEffectsImpl_.shutdown();
    canvasSprite_.reset();
    canvas_.reset();
    canvasDefaultViewActive_ = true;
}

}  // namespace ludork::global::system_impl
