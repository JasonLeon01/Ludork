#include "MapRendererImpl.hpp"

#include <EngineState.hpp>
#include <Graphics/HueConstants.hpp>
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
        std::clamp(tint.a * value.material.opacity * value.presentationOpacity,
                   0.0f, 255.0f));
    sprite.setColor(tint);
    sf::RenderStates spriteStates = states;
    spriteStates.transform.combine(value.transform);
    if (value.shaderError) {
        sprite.setColor({255, 0, 255, tint.a});
        target.draw(sprite, spriteStates);
        return;
    }
    std::shared_ptr<sf::Shader> effect = value.shader;
    if (!effect && !value.shaderPath.empty()) {
        effect = shader(value.shaderPath);
    }
    const float hue = std::fmod(std::fmod(value.hue, 360.0f) + 360.0f, 360.0f);
    std::shared_ptr<sf::Shader> hueShader;
    if (hue > ludork::engine::graphics::HueEpsilon &&
        360.0f - hue > ludork::engine::graphics::HueEpsilon) {
        hueShader = shader(ludork::engine::graphics::HueShaderPath);
        hueShader->setUniform("screenTex", sf::Shader::CurrentTexture);
        hueShader->setUniform("hue", hue);
    }
    if (effect) {
        effect->setUniform("texture", sf::Shader::CurrentTexture);
        effect->setUniform("time", time);
        effect->setUniform("textureSize",
                           sf::Vector2f(value.texture->getSize()));
        effect->setUniform(
            "textureRect",
            sf::Glsl::Vec4(static_cast<float>(value.textureRect.position.x),
                           static_cast<float>(value.textureRect.position.y),
                           static_cast<float>(value.textureRect.size.x),
                           static_cast<float>(value.textureRect.size.y)));
    }
    if (effect || hueShader) {
        const sf::Vector2u size(static_cast<unsigned int>(std::max(
                                    1, std::abs(value.textureRect.size.x))),
                                static_cast<unsigned int>(std::max(
                                    1, std::abs(value.textureRect.size.y))));
        sf::RenderTexture& buffer = Impl::target(spriteEffect_, size);
        buffer.setView(buffer.getDefaultView());
        buffer.clear(sf::Color::Transparent);
        sprite.setColor(sf::Color::White);
        sf::RenderStates effectStates(sf::BlendNone);
        effectStates.shader = effect ? effect.get() : hueShader.get();
        buffer.draw(sprite, effectStates);
        buffer.display();
        const sf::Texture* result = &buffer.getTexture();
        if (effect && hueShader) {
            sf::RenderTexture& hueBuffer = Impl::target(spriteHue_, size);
            hueBuffer.setView(hueBuffer.getDefaultView());
            hueBuffer.clear(sf::Color::Transparent);
            effectStates.shader = hueShader.get();
            hueBuffer.draw(sf::Sprite(buffer.getTexture()), effectStates);
            hueBuffer.display();
            result = &hueBuffer.getTexture();
        }
        sf::Sprite composed(*result);
        composed.setColor(tint);
        spriteStates.shader = nullptr;
        target.draw(composed, spriteStates);
        return;
    }
    target.draw(sprite, spriteStates);
}

void MapRenderer::Impl::drawContent(const Scene& scene,
                                    sf::RenderTarget& target,
                                    const sf::RenderStates& states,
                                    const Options& options,
                                    const DrawHooks& hooks) {
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
                layerStates.shader = layer->getShader().get();
                destination->draw(*layer, layerStates);
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
