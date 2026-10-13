#include "MapRendererImpl.hpp"

#include <EngineState.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <unordered_set>

void MapRenderer::Impl::setTileMask(TileLayer& layer, const Region& region,
                                    const View& view, sf::RenderTarget& target,
                                    bool transmission) {
    LayerMasks& masks = layerMasks_[&layer];
    const std::shared_ptr<sf::Image> block = layer.getLightBlockImage();
    if (!masks.lightBlock || masks.revision != layer.getContentRevision() ||
        masks.source != block) {
        masks.revision = layer.getContentRevision();
        masks.source = block;
        masks.lightBlock = std::make_shared<sf::Texture>(*block);
        masks.reflection =
            std::make_shared<sf::Texture>(*layer.getReflectionStrengthImage());
        masks.ignoreLighting =
            std::make_shared<sf::Texture>(*layer.getIgnoreLightingImage());
    }
    const std::shared_ptr<sf::Shader> mask =
        shader("/Game/Assets/Shaders/Global/TilemapLightMask.frag");
    mask->setUniform("texture", sf::Shader::CurrentTexture);
    mask->setUniform("lightBlockTex", *masks.lightBlock);
    mask->setUniform("reflectionStrengthTex", *masks.reflection);
    mask->setUniform("ignoreLightingTex", *masks.ignoreLighting);
    mask->setUniform("lightBlockSize",
                     sf::Vector2f(static_cast<float>(EngineState::CellSize),
                                  static_cast<float>(EngineState::CellSize)));
    mask->setUniform("mapSize", sf::Vector2f(layer.getGridSize()));
    mask->setUniform("transmissionMode", transmission ? 1.0f : 0.0f);
    mask->setUniform("worldMode", 1.0f);
    mask->setUniform("maskTargetSize", sf::Vector2f(target.getSize()));
    mask->setUniform("maskViewSize", view.worldRect.size);
    mask->setUniform("maskViewPosition", view.worldRect.position);
    mask->setUniform("maskViewRotation", view.rotation.asDegrees());
    mask->setUniform("regionPosition",
                     sf::Vector2f(region.cellOrigin) *
                         static_cast<float>(EngineState::CellSize));
}

void MapRenderer::Impl::drawSpriteMask(const Sprite& sprite,
                                       sf::RenderTarget& target,
                                       bool transmission) {
    if (!sprite.texture || !sprite.visible || !sprite.affectsLighting) {
        return;
    }
    const std::shared_ptr<sf::Shader> mask =
        shader("/Game/Assets/Shaders/Global/LightMask.frag");
    mask->setUniform("texture", sf::Shader::CurrentTexture);
    mask->setUniform("lightBlock", sprite.material.lightBlock);
    mask->setUniform(
        "reflectionStrength",
        sprite.material.mirror ? sprite.material.reflectionStrength : 0.0f);
    mask->setUniform("ignoreLighting",
                     sprite.material.ignoreLighting ? 1.0f : 0.0f);
    mask->setUniform("transmissionMode", transmission ? 1.0f : 0.0f);
    sf::Sprite drawable(*sprite.texture, sprite.textureRect);
    sf::Color tint = sprite.colour;
    tint.a = static_cast<std::uint8_t>(
        std::clamp(tint.a * sprite.material.opacity, 0.0f, 255.0f));
    drawable.setColor(tint);
    sf::RenderStates states = transmission ? sf::RenderStates(sf::BlendMultiply)
                                           : canvasRenderStates();
    states.shader = mask.get();
    states.transform = sprite.transform;
    target.draw(drawable, states);
}

void MapRenderer::Impl::renderMasks(const Scene& scene, const View& view) {
    sf::RenderTexture& surface = target(surface_, view.pixelSize);
    surface.setView(nativeView(view));
    surface.clear(sf::Color::Transparent);
    const std::vector<const Region*> regions = orderedRegions(scene);
    std::set<int> priorities{0};
    std::unordered_map<std::string, int> regionPriorities;
    std::unordered_set<const TileLayer*> retainedLayers;
    for (const Region* region : regions) {
        priorities.insert(region->priority);
        regionPriorities.emplace(region->id, region->priority);
    }
    for (const std::string& name : scene.layerOrder) {
        for (const int priority : priorities) {
            for (const Region* region : regions) {
                const std::shared_ptr<TileLayer> layer =
                    region->tilemap->getLayer(name);
                if (region->priority != priority || !layer ||
                    !layer->getVisible()) {
                    continue;
                }
                retainedLayers.insert(layer.get());
                setTileMask(*layer, *region, view, surface, false);
                sf::RenderStates states = canvasRenderStates();
                states.shader =
                    shader("/Game/Assets/Shaders/Global/TilemapLightMask.frag")
                        .get();
                states.transform.translate(
                    sf::Vector2f(region->cellOrigin) *
                    static_cast<float>(EngineState::CellSize));
                surface.draw(*layer, states);
            }
            for (const Sprite& sprite : scene.sprites) {
                const auto found = regionPriorities.find(sprite.regionId);
                const int spritePriority =
                    found == regionPriorities.end() ? 0 : found->second;
                if (spritePriority == priority && sprite.layer == name &&
                    layerVisible(scene, sprite)) {
                    drawSpriteMask(sprite, surface, false);
                }
            }
        }
    }
    std::erase_if(layerMasks_, [&retainedLayers](const auto& entry) {
        return !retainedLayers.contains(entry.first);
    });
    surface.display();
}

void MapRenderer::Impl::renderTransmission(const Scene& scene, const View& view,
                                           const std::vector<Light>& lights,
                                           const Options& options) {
    const float cell = static_cast<float>(EngineState::CellSize);
    const sf::Vector2f centre =
        view.worldRect.position + view.worldRect.size * 0.5f;
    const float radians = view.rotation.asRadians();
    const sf::Vector2f extent(
        (std::abs(std::cos(radians)) * view.worldRect.size.x +
         std::abs(std::sin(radians)) * view.worldRect.size.y) *
            0.5f,
        (std::abs(std::sin(radians)) * view.worldRect.size.x +
         std::abs(std::cos(radians)) * view.worldRect.size.y) *
            0.5f);
    sf::Vector2f minimum = centre - extent;
    sf::Vector2f maximum = centre + extent;
    for (const Light& light : lights) {
        minimum.x = std::min(minimum.x, light.position.x - light.radius - cell);
        minimum.y = std::min(minimum.y, light.position.y - light.radius - cell);
        maximum.x = std::max(maximum.x, light.position.x + light.radius + cell);
        maximum.y = std::max(maximum.y, light.position.y + light.radius + cell);
    }
    const sf::Vector2i origin(static_cast<int>(std::floor(minimum.x / cell)),
                              static_cast<int>(std::floor(minimum.y / cell)));
    const sf::Vector2u cells(
        static_cast<unsigned int>(
            std::max(1.0f, std::ceil(maximum.x / cell) - origin.x)),
        static_cast<unsigned int>(
            std::max(1.0f, std::ceil(maximum.y / cell) - origin.y)));
    const sf::FloatRect rect(sf::Vector2f(origin) * cell,
                             sf::Vector2f(cells) * cell);
    const float pixelDensity =
        std::min(
            {1.0f, static_cast<float>(view.pixelSize.x) / view.worldRect.size.x,
             static_cast<float>(view.pixelSize.y) / view.worldRect.size.y}) *
        options.lightingScale;
    std::size_t signature = 0;
    const auto mix = [&signature](std::size_t value) {
        signature ^= value + 0x9e3779b9 + (signature << 6) + (signature >> 2);
    };
    mix(std::hash<int>{}(origin.x));
    mix(std::hash<int>{}(origin.y));
    mix(cells.x);
    mix(cells.y);
    mix(std::hash<float>{}(pixelDensity));
    for (const Region& region : scene.regions) {
        mix(std::hash<int>{}(region.cellOrigin.x));
        mix(std::hash<int>{}(region.cellOrigin.y));
        mix(region.revision);
        if (!region.tilemap) {
            continue;
        }
        for (const std::string& name : scene.layerOrder) {
            const std::shared_ptr<TileLayer> layer =
                region.tilemap->getLayer(name);
            mix(std::hash<const void*>{}(layer.get()));
            if (layer) {
                mix(layer->getContentRevision());
                mix(layer->getDrawRevision());
                mix(layer->getVisible());
            }
        }
    }
    if (transmission_ && signature == transmissionSignature_) {
        return;
    }
    const float maximumTextureSize =
        static_cast<float>(sf::Texture::getMaximumSize());
    const float scale =
        std::min({1.0f, pixelDensity, maximumTextureSize / rect.size.x,
                  maximumTextureSize / rect.size.y});
    const sf::Vector2u pixels(static_cast<unsigned int>(std::max(
                                  1.0f, std::ceil(rect.size.x * scale))),
                              static_cast<unsigned int>(std::max(
                                  1.0f, std::ceil(rect.size.y * scale))));
    View maskView{rect, sf::degrees(0.0f), pixels};
    sf::RenderTexture& transmission = target(transmission_, pixels);
    transmission.setView(nativeView(maskView));
    transmission.clear(sf::Color::White);
    sf::Image occupancy(cells, sf::Color::Black);
    for (const Region& region : scene.regions) {
        if (!region.tilemap) {
            continue;
        }
        for (const std::string& name : scene.layerOrder) {
            const std::shared_ptr<TileLayer> layer =
                region.tilemap->getLayer(name);
            if (!layer || !layer->getVisible()) {
                continue;
            }
            layer->setAnimationTime(options.time);
            setTileMask(*layer, region, maskView, transmission, true);
            sf::RenderStates states(sf::BlendMultiply);
            states.shader =
                shader("/Game/Assets/Shaders/Global/TilemapLightMask.frag")
                    .get();
            states.transform.translate(sf::Vector2f(region.cellOrigin) * cell);
            transmission.draw(*layer, states);
            const auto& grid = layer->getLightBlockMapView();
            const int startX = std::max(0, origin.x - region.cellOrigin.x);
            const int startY = std::max(0, origin.y - region.cellOrigin.y);
            const int endX = std::min(
                static_cast<int>(layer->getGridSize().x),
                origin.x + static_cast<int>(cells.x) - region.cellOrigin.x);
            const int endY = std::min(
                static_cast<int>(layer->getGridSize().y),
                origin.y + static_cast<int>(cells.y) - region.cellOrigin.y);
            for (int y = startY; y < endY; ++y) {
                for (int x = startX; x < endX; ++x) {
                    if (grid[y][x] > 0.0f) {
                        occupancy.setPixel(
                            {static_cast<unsigned int>(x + region.cellOrigin.x -
                                                       origin.x),
                             cells.y - 1u -
                                 static_cast<unsigned int>(
                                     y + region.cellOrigin.y - origin.y)},
                            sf::Color::White);
                    }
                }
            }
        }
    }
    transmission.display();
    occupancy_ = std::make_shared<sf::Texture>(occupancy);
    transmissionRect_ = rect;
    transmissionSignature_ = signature;
    ++transmissionGeneration_;
}
