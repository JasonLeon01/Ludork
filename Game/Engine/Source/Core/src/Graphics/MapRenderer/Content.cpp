#include "MapRendererImpl.hpp"

#include <EngineState.hpp>
#include <Runtime/Graphics/ScopedSceneLighting.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <set>

void MapRenderer::Impl::drawSprite(const Sprite& value,
                                   sf::RenderTarget& target,
                                   const sf::RenderStates& states, float time) {
    if (!value.visible || !value.texture || value.presentationOpacity <= 0.0f) {
        return;
    }
    sf::Sprite sprite(*value.texture, value.textureRect);
    sf::Color tint = value.colour;
    tint.a = static_cast<std::uint8_t>(
        std::clamp(tint.a * value.presentationOpacity, 0.0f, 255.0f));
    sprite.setColor(tint);
    sf::RenderStates spriteStates = states;
    spriteStates.transform.combine(value.transform);
    const auto resource =
        value.materialResource
            ? value.materialResource
            : ludork::engine::materials::MaterialResource::load(value.material);
    auto context = materialContext_;
    context.time = time;
    context.textureSize = value.texture->getSize();
    context.textureRect = value.textureRect;
    context.targetSize = target.getSize();
    context.worldTransform = spriteStates.transform;
    const auto program =
        materialPrograms_.bind(*resource, value.materialParameters, nullptr, {},
                               materialPass_, context);
    spriteStates.shader = program.get();
    target.draw(sprite, spriteStates);
}

void MapRenderer::Impl::drawContent(const Scene& scene,
                                    sf::RenderTarget& target,
                                    const sf::RenderStates& states,
                                    const Options& options,
                                    const DrawHooks& hooks) {
    ludork::runtime::graphics::SceneLightingContext lightingContext;
    lightingContext.ambientLight = materialContext_.ambientLight;
    lightingContext.directLight = materialContext_.directLight;
    lightingContext.targetSize = target.getSize();
    lightingContext.worldView = materialContext_.worldView;
    lightingContext.viewRotation = materialContext_.viewRotation;
    lightingContext.enabled = materialContext_.lighting;
    const ludork::runtime::graphics::ScopedSceneLighting lightingScope(
        lightingContext);
    const std::vector<const Region*> regions = orderedRegions(scene);
    std::set<int> priorities{0};
    std::unordered_map<std::string, int> regionPriorities;
    for (const Region* region : regions) {
        priorities.insert(region->priority);
        regionPriorities.emplace(region->id, region->priority);
    }
    for (const std::string& name : scene.layerOrder) {
        const float opacity =
            options.activeLayer.empty() || options.activeLayer == name
                ? 1.0f
                : std::clamp(options.inactiveLayerOpacity, 0.0f, 1.0f);
        sf::RenderTarget* destination = &target;
        if (opacity < 1.0f) {
            sf::RenderTexture& buffer =
                Impl::target(layerEffect_, target.getSize());
            buffer.setView(target.getView());
            buffer.clear(sf::Color::Transparent);
            destination = &buffer;
        }
        for (const int priority : priorities) {
            for (const Region* region : regions) {
                if (region->priority != priority) {
                    continue;
                }
                const std::shared_ptr<TileLayer> layer =
                    region->tilemap->getLayer(name);
                if (!layer || !layer->getVisible()) {
                    continue;
                }
                layer->setAnimationTime(options.time);
                sf::RenderStates layerStates = states;
                layerStates.transform.translate(
                    sf::Vector2f(region->cellOrigin) *
                    static_cast<float>(EngineState::CellSize));
                layer->drawMaterialPass(*destination, layerStates,
                                        materialPrograms_, materialPass_,
                                        materialContext_);
            }
            for (const Sprite& sprite : scene.sprites) {
                const auto found = regionPriorities.find(sprite.regionId);
                const int spritePriority =
                    found == regionPriorities.end() ? 0 : found->second;
                if (spritePriority != priority || sprite.layer != name ||
                    !layerVisible(scene, sprite)) {
                    continue;
                }
                if (hooks.beforeSprite) {
                    hooks.beforeSprite(sprite.id, *destination, states);
                }
                drawSprite(sprite, *destination, states, options.time);
                if (hooks.afterSprite) {
                    hooks.afterSprite(sprite.id, *destination, states);
                }
            }
        }
        if (hooks.afterLayer) {
            hooks.afterLayer(name, *destination, states);
        }
        if (opacity < 1.0f) {
            layerEffect_->display();
            sf::Sprite image(layerEffect_->getTexture());
            const std::uint8_t alpha =
                static_cast<std::uint8_t>(opacity * 255.0f);
            image.setColor({alpha, alpha, alpha, alpha});
            const sf::View previous = target.getView();
            target.setView(target.getDefaultView());
            target.draw(image, premultipliedRenderStates());
            target.setView(previous);
        }
    }
}
