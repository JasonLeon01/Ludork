#include "MapRendererImpl.hpp"

#include <EngineState.hpp>
#include <Runtime/Graphics/ScopedSceneLighting.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

bool MapRenderer::Impl::hasReflection(const Scene& scene,
                                      const View& view) const {
    sf::Transform rotation;
    rotation.rotate(view.rotation,
                    view.worldRect.position + view.worldRect.size * 0.5f);
    const sf::FloatRect visible = rotation.transformRect(view.worldRect);
    for (const auto& sprite : scene.sprites) {
        if (!sprite.texture || sprite.presentationOpacity <= 0.0f ||
            !layerVisible(scene, sprite)) {
            continue;
        }
        const auto resource =
            sprite.materialResource
                ? sprite.materialResource
                : ludork::engine::materials::MaterialResource::load(
                      sprite.material);
        const sf::Vector2f offset = resource->maximumVertexOffset();
        if (!sprite.transform
                 .transformRect(
                     {-offset,
                      sf::Vector2f(std::abs(sprite.textureRect.size.x),
                                   std::abs(sprite.textureRect.size.y)) +
                          offset * 2.0f})
                 .findIntersection(visible)) {
            continue;
        }
        if (ludork::engine::materials::MaterialCompiler::analyse(
                *resource, sprite.materialParameters)
                .mayReflect) {
            return true;
        }
    }
    for (const auto& region : scene.regions) {
        if (!region.tilemap) {
            continue;
        }
        for (const auto& name : scene.layerOrder) {
            const auto layer = region.tilemap->getLayer(name);
            if (layer && layer->getVisible() &&
                layer->hasReflectiveMaterial(
                    {visible.position -
                         sf::Vector2f(region.cellOrigin) *
                             static_cast<float>(EngineState::CellSize),
                     visible.size})) {
                return true;
            }
        }
    }
    return false;
}

void MapRenderer::Impl::drawOverlay(const sf::Texture& overlay,
                                    sf::RenderTarget& destination,
                                    const View& view) {
    Sprite sprite;
    sprite.texture = std::shared_ptr<sf::Texture>(
        const_cast<sf::Texture*>(&overlay), [](sf::Texture*) {});
    sprite.textureRect = {{0, 0}, sf::Vector2i(overlay.getSize())};
    sf::Transformable transform;
    transform.setOrigin(sf::Vector2f(overlay.getSize()) * 0.5f);
    transform.setPosition(view.worldRect.position + view.worldRect.size * 0.5f);
    transform.setScale(
        view.worldRect.size.componentWiseDiv(sf::Vector2f(overlay.getSize())));
    transform.setRotation(view.rotation);
    sprite.transform = transform.getTransform();
    const auto saved = materialContext_;
    materialContext_.premultipliedSource = true;
    drawSprite(sprite, destination, canvasRenderStates(),
               materialContext_.time);
    materialContext_ = saved;
}

const sf::RenderTexture& MapRenderer::Impl::render(const Scene& scene,
                                                   const View& view,
                                                   const Options& options,
                                                   const DrawHooks& hooks,
                                                   const sf::Texture* overlay) {
    validate(scene, view, options);
    statistics_ = {};
    const std::uint64_t materialDraws = materialPrograms_.bindCount();
    materialContext_ = {};
    materialContext_.time = options.time;
    materialContext_.worldView = view.worldRect;
    materialContext_.viewRotation = view.rotation.asDegrees();
    materialContext_.cellSize = static_cast<float>(EngineState::CellSize);
    materialContext_.targetSize = view.pixelSize;
    const bool reflection = hasReflection(scene, view);
    renderAmbient(scene, view, options);
    if (reflection) {
        renderMasks(scene, view);
    }
    renderLighting(scene, view, options);
    DrawHooks frameHooks = hooks;
    if (reflection && hooks.afterLayer) {
        std::erase_if(layerCallbackBuffers_, [&scene](const auto& entry) {
            return std::find(scene.layerOrder.begin(), scene.layerOrder.end(),
                             entry.first) == scene.layerOrder.end();
        });
        const ludork::runtime::graphics::ScopedSceneLighting unlit({});
        for (const std::string& name : scene.layerOrder) {
            sf::RenderTexture& buffer =
                target(layerCallbackBuffers_[name], view.pixelSize);
            buffer.setView(nativeView(view));
            buffer.clear(sf::Color::Transparent);
            hooks.afterLayer(name, buffer, canvasRenderStates());
            buffer.display();
        }
        frameHooks.afterLayer = [this, &view](const std::string& name,
                                              sf::RenderTarget& destination,
                                              const sf::RenderStates&) {
            drawOverlay(layerCallbackBuffers_.at(name)->getTexture(),
                        destination, view);
        };
    }
    if (reflection) {
        statistics_.reflectionPasses = 1;
        sf::RenderTexture& source = target(content_, view.pixelSize);
        source.setView(nativeView(view));
        source.clear(sf::Color::Transparent);
        materialPass_ =
            ludork::engine::materials::MaterialCompiler::Pass::Source;
        drawContent(scene, source, canvasRenderStates(), options, frameHooks);
        if (overlay) {
            drawOverlay(*overlay, source, view);
        }
        source.display();
    }
    sf::RenderTexture& output = target(output_, view.pixelSize);
    output.setView(output.getDefaultView());
    output.clear(sf::Color::Transparent);
    if (options.showPanorama && scene.panorama) {
        drawPanorama(scene, view, options);
    }
    output.setView(nativeView(view));
    materialContext_.lighting = options.lighting == LightingMode::Lit;
    materialContext_.ambientLight = &ambient_->getTexture();
    materialContext_.directLight = &directLight_->getTexture();
    materialContext_.surfaceMask =
        reflection ? &surface_->getTexture() : nullptr;
    materialContext_.reflectionSource =
        reflection ? &content_->getTexture() : nullptr;
    materialContext_.reflection = reflection;
    materialPass_ = ludork::engine::materials::MaterialCompiler::Pass::Color;
    drawContent(scene, output, canvasRenderStates(), options, frameHooks);
    if (overlay) {
        drawOverlay(*overlay, output, view);
    }
    output.setView(output.getDefaultView());
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
    statistics_.materialDrawCalls =
        materialPrograms_.bindCount() - materialDraws;
    for (const auto* texture :
         {content_.get(), output_.get(), surface_.get(), ambient_.get(),
          transmission_.get(), dynamicTransmission_.get(), directLight_.get(),
          layerEffect_.get()}) {
        if (texture) {
            ++statistics_.residentRenderTextures;
        }
    }
    statistics_.residentRenderTextures +=
        static_cast<unsigned int>(layerCallbackBuffers_.size());
    for (const auto& cache : lightCaches_) {
        if (cache.texture) {
            ++statistics_.residentRenderTextures;
        }
    }
    materialContext_ = {};
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
