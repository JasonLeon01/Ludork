#include "MapRendererImpl.hpp"

#include <EngineState.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

const sf::RenderTexture& MapRenderer::Impl::compose(
    const Scene& scene, const View& view, const Options& options,
    const sf::Texture& content) {
    validate(scene, view, options);
    renderAmbient(scene, view, options);
    renderMasks(scene, view);
    renderLighting(scene, view, options);
    sf::RenderTexture& output = target(output_, view.pixelSize);
    output.setView(output.getDefaultView());
    output.clear(sf::Color::Transparent);
    if (options.showPanorama && scene.panorama) {
        drawPanorama(scene, view, options);
    }
    const std::shared_ptr<sf::Shader> material =
        shader("/Game/Assets/Shaders/Global/MapMaterial.frag");
    setViewUniforms(*material, scene, view);
    material->setUniform("texture", sf::Shader::CurrentTexture);
    material->setUniform("surfaceMask", surface_->getTexture());
    material->setUniform("directLight", directLight_->getTexture());
    material->setUniform("staticDirectLight", directLight_->getTexture());
    material->setUniform("useStaticDirectLight", 0.0f);
    material->setUniform("ambientLight", ambient_->getTexture());
    material->setUniform("preserveSourceAlpha", 1.0f);
    sf::Sprite image(content);
    image.setScale(sf::Vector2f(view.pixelSize)
                       .componentWiseDiv(sf::Vector2f(content.getSize())));
    sf::RenderStates states = premultipliedRenderStates();
    states.shader = material.get();
    output.draw(image, states);
    if (options.showFog) {
        drawFog(scene.fog,
                {{0.0f, 0.0f},
                 sf::Vector2f(scene.cellSize) *
                     static_cast<float>(EngineState::CellSize)},
                scene, view, options);
        for (const Region& region : scene.regions) {
            if (region.fog && region.tilemap) {
                drawFog(*region.fog,
                        {sf::Vector2f(region.cellOrigin) *
                             static_cast<float>(EngineState::CellSize),
                         sf::Vector2f(region.tilemap->getSize()) *
                             static_cast<float>(EngineState::CellSize)},
                        scene, view, options);
            }
        }
    }
    output.display();
    return output;
}

void MapRenderer::Impl::drawPanorama(const Scene& scene, const View& view,
                                     const Options& options) {
    const sf::Vector2f textureSize(scene.panorama->getSize());
    if (textureSize.x <= 0.0f || textureSize.y <= 0.0f) {
        return;
    }
    const sf::Vector2f logicalSize = view.worldRect.size;
    const float cover =
        std::max(logicalSize.x / textureSize.x, logicalSize.y / textureSize.y);
    const sf::Vector2f overflow = textureSize * cover - logicalSize;
    const sf::Vector2f mapSize = sf::Vector2f(scene.cellSize) *
                                 static_cast<float>(EngineState::CellSize);
    const auto scroll = [](float position, float total, float visible) {
        return total <= visible
                   ? 0.5f
                   : std::clamp(position / (total - visible), 0.0f, 1.0f);
    };
    const sf::Vector2f pixelScale =
        sf::Vector2f(view.pixelSize).componentWiseDiv(logicalSize);
    sf::Sprite panorama(*scene.panorama);
    panorama.setScale(pixelScale * cover);
    panorama.setPosition(
        {-overflow.x *
             scroll(view.worldRect.position.x, mapSize.x, logicalSize.x) *
             pixelScale.x,
         -overflow.y *
             scroll(view.worldRect.position.y, mapSize.y, logicalSize.y) *
             pixelScale.y});
    sf::RenderStates states;
    const std::shared_ptr<sf::Shader> effect =
        shader("/Game/Assets/Shaders/Global/MapPanorama.frag");
    effect->setUniform("texture", sf::Shader::CurrentTexture);
    effect->setUniform("ambientLight", ambient_->getTexture());
    effect->setUniform("pixelSize", sf::Vector2f(view.pixelSize));
    states.shader = effect.get();
    output_->draw(panorama, states);
}

void MapRenderer::Impl::drawFog(const MapFogSettings& settings,
                                const sf::FloatRect& bounds, const Scene& scene,
                                const View& view, const Options& options) {
    if (!settings.enabled || settings.density <= 0.0f) {
        return;
    }
    if (!std::isfinite(settings.density) || settings.density < 0.0f ||
        settings.density > 1.0f || !std::isfinite(settings.scale) ||
        settings.scale <= 0.0f || !std::isfinite(settings.velocity.x) ||
        !std::isfinite(settings.velocity.y) ||
        !std::isfinite(settings.turbulence) || settings.turbulence < 0.0f ||
        settings.turbulence > 1.0f) {
        throw std::invalid_argument(
            "Map fog requires finite density and turbulence in [0, 1], "
            "positive scale and finite velocity");
    }
    const std::shared_ptr<sf::Shader> fog =
        shader("/Game/Assets/Shaders/Global/MapFog.frag");
    fog->setUniform("screenSize", view.worldRect.size);
    fog->setUniform("viewPos", view.worldRect.position);
    const float angle = view.rotation.asRadians();
    fog->setUniform("viewSinCos",
                    sf::Vector2f(std::sin(angle), std::cos(angle)));
    fog->setUniform("pixelSize", sf::Vector2f(view.pixelSize));
    fog->setUniform("regionOrigin", bounds.position);
    fog->setUniform("regionSize", bounds.size);
    fog->setUniform("fogColour", sf::Glsl::Vec4(settings.colour));
    fog->setUniform("density", settings.density);
    fog->setUniform("noiseScale", settings.scale);
    fog->setUniform("velocity", settings.velocity);
    fog->setUniform("turbulence", settings.turbulence);
    fog->setUniform("seed", static_cast<float>(settings.seed));
    fog->setUniform("time", options.time);
    fog->setUniform("lit", options.lighting == LightingMode::Lit ? 1.0f : 0.0f);
    fog->setUniform("ambientLight", ambient_->getTexture());
    fog->setUniform("directLight", directLight_->getTexture());
    sf::RectangleShape rectangle(sf::Vector2f(view.pixelSize));
    sf::RenderStates states = canvasRenderStates();
    states.shader = fog.get();
    output_->draw(rectangle, states);
}

void MapRenderer::Impl::renderAmbient(const Scene& scene, const View& view,
                                      const Options& options) {
    sf::RenderTexture& ambient = target(ambient_, view.pixelSize);
    ambient.setView(nativeView(view));
    const auto tint = [](const sf::Color& value) {
        const sf::Vector3f channels = colour(value, true);
        return sf::Color(static_cast<std::uint8_t>(channels.x * 255.0f),
                         static_cast<std::uint8_t>(channels.y * 255.0f),
                         static_cast<std::uint8_t>(channels.z * 255.0f), 255);
    };
    ambient.clear(options.lighting == LightingMode::Lit
                      ? tint(scene.ambientLight)
                      : sf::Color::White);
    if (options.lighting == LightingMode::Lit) {
        for (const Region* region : orderedRegions(scene)) {
            if (!region->ambientLight) {
                continue;
            }
            sf::RectangleShape rectangle(
                sf::Vector2f(region->tilemap->getSize()) *
                static_cast<float>(EngineState::CellSize));
            rectangle.setPosition(sf::Vector2f(region->cellOrigin) *
                                  static_cast<float>(EngineState::CellSize));
            rectangle.setFillColor(tint(*region->ambientLight));
            ambient.draw(rectangle, sf::RenderStates(sf::BlendNone));
        }
    }
    ambient.display();
}
