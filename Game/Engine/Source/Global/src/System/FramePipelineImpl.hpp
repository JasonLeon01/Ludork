#pragma once

#include "GraphicsShaderSink.hpp"
#include "ScreenEffectsImpl.hpp"
#include "TransitionImpl.hpp"
#include <System/GraphicsTypes.hpp>
#include <SFML/Graphics.hpp>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ludork::global::system_impl {

class DisplayImpl;

class FramePipelineImpl : private GraphicsShaderSink {
public:
    FramePipelineImpl();
    ScreenEffectsImpl& screenEffects();
    TransitionImpl& transition();
    void initCanvas(const sf::Vector2u& size,
                    bool preserveTransitionBackground = false);
    void draw(const sf::Drawable& drawable, sf::Shader* shader);
    void composeFrame(float deltaTime, sf::RenderTarget* target);
    void present(DisplayImpl& display);
    bool completeFrame(bool hasWindow);
    void addGraphicsShader(
        const std::shared_ptr<sf::Shader>& shader,
        std::optional<ShaderUniforms> uniforms = std::nullopt);
    void removeGraphicsShader(const std::shared_ptr<sf::Shader>& shader);
    void removeAllGraphicsShaders();
    void removeGraphicsShaderAt(int index);
    void setWindowMapView(const sf::IntRect& rect);
    void setWindowDefaultView();
    sf::RenderTexture* getCanvas();
    void clearCanvas();
    sf::Vector2u getCanvasSize() const;
    void initializeGraphics();
    void rebuildTargets(const sf::Vector2u& size, float renderScale);
    void reset();
    void shutdown() noexcept;

private:
    void applyGraphicsShadersLength();
    void setShaderUniform(sf::Shader& shader, const std::string& name,
                          const ShaderUniformValue& value);
    bool shadersAvailable();
    void addEffectShader(const std::shared_ptr<sf::Shader>& shader) override;
    void removeEffectShader(const std::shared_ptr<sf::Shader>& shader) override;
    bool viewsEqual(const sf::View& left, const sf::View& right);
    bool canvasDefaultViewActive_ = true;
    std::unique_ptr<sf::RenderTexture> canvas_;
    std::optional<sf::Sprite> canvasSprite_;

    std::vector<std::unique_ptr<sf::RenderTexture>> graphicsCanvases_;
    std::vector<std::shared_ptr<sf::Shader>> graphicsShaders_;
    std::mutex presentMutex_;
    ScreenEffectsImpl screenEffectsImpl_;
    TransitionImpl transitionImpl_;
};

FramePipelineImpl& framePipelineImpl();

}  // namespace ludork::global::system_impl
