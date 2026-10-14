#include "MapRendererImpl.hpp"

#include <EngineState.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <unordered_set>

void MapRenderer::Impl::drawSpriteMask(const Sprite& sprite,
                                       sf::RenderTarget& target,
                                       bool transmission) {
    if (!sprite.texture || !sprite.visible || !sprite.affectsLighting) {
        return;
    }
    const auto resource =
        sprite.materialResource
            ? sprite.materialResource
            : ludork::engine::materials::MaterialResource::load(
                  sprite.material);
    auto context = materialContext_;
    context.textureSize = sprite.texture->getSize();
    context.textureRect = sprite.textureRect;
    context.worldTransform = sprite.transform;
    context.targetSize = target.getSize();
    context.lighting = false;
    context.reflection = false;
    const auto program = materialPrograms_.bind(
        *resource, sprite.materialParameters, nullptr, {},
        transmission
            ? ludork::engine::materials::MaterialCompiler::Pass::Transmission
            : ludork::engine::materials::MaterialCompiler::Pass::Surface,
        context);
    sf::Sprite drawable(*sprite.texture, sprite.textureRect);
    drawable.setColor(sprite.colour);
    sf::RenderStates states = transmission ? sf::RenderStates(sf::BlendMultiply)
                                           : canvasRenderStates();
    states.shader = program.get();
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
                sf::RenderStates states = canvasRenderStates();
                states.transform.translate(
                    sf::Vector2f(region->cellOrigin) *
                    static_cast<float>(EngineState::CellSize));
                layer->setAnimationTime(materialContext_.time);
                layer->drawMaterialPass(
                    surface, states, materialPrograms_,
                    ludork::engine::materials::MaterialCompiler::Pass::Surface,
                    materialContext_);
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
                layer->setAnimationTime(options.time);
                mix(std::hash<std::string>{}(layer->material));
                mix(layer->getContentRevision());
                mix(layer->getDrawRevision());
                mix(layer->getVisible());
                if (layer->hasAnimatedOcclusion()) {
                    mix(std::hash<float>{}(options.time));
                }
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
            sf::RenderStates states(sf::BlendMultiply);
            states.transform.translate(sf::Vector2f(region.cellOrigin) * cell);
            auto context = materialContext_;
            context.worldView = maskView.worldRect;
            context.viewRotation = 0.0f;
            layer->drawMaterialPass(
                transmission, states, materialPrograms_,
                ludork::engine::materials::MaterialCompiler::Pass::Transmission,
                context);
            const sf::Vector2f displacement = layer->maximumMaterialOffset();
            const int paddingX =
                static_cast<int>(std::ceil(displacement.x / cell));
            const int paddingY =
                static_cast<int>(std::ceil(displacement.y / cell));
            const int startX =
                std::max(0, origin.x - region.cellOrigin.x - paddingX);
            const int startY =
                std::max(0, origin.y - region.cellOrigin.y - paddingY);
            const int endX = std::min(static_cast<int>(layer->getGridSize().x),
                                      origin.x + static_cast<int>(cells.x) -
                                          region.cellOrigin.x + paddingX);
            const int endY = std::min(static_cast<int>(layer->getGridSize().y),
                                      origin.y + static_cast<int>(cells.y) -
                                          region.cellOrigin.y + paddingY);
            for (int y = startY; y < endY; ++y) {
                for (int x = startX; x < endX; ++x) {
                    if (layer->hasContent({x, y})) {
                        const int centreX = x + region.cellOrigin.x - origin.x;
                        const int centreY = y + region.cellOrigin.y - origin.y;
                        for (int maskY = std::max(0, centreY - paddingY);
                             maskY <= std::min(static_cast<int>(cells.y) - 1,
                                               centreY + paddingY);
                             ++maskY) {
                            for (int maskX = std::max(0, centreX - paddingX);
                                 maskX <=
                                 std::min(static_cast<int>(cells.x) - 1,
                                          centreX + paddingX);
                                 ++maskX) {
                                occupancy.setPixel(
                                    {static_cast<unsigned>(maskX),
                                     cells.y - 1u -
                                         static_cast<unsigned>(maskY)},
                                    sf::Color::White);
                            }
                        }
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
