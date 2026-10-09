#include "GameMapRendererImpl.hpp"

#include <Manager/ShaderManager.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

void GameMapRendererImpl::setPreviewSprites(
    const std::vector<PreviewSprite>& sprites) {
    if (!previewOnly) {
        throw std::logic_error("Preview sprites require a preview renderer");
    }
    std::vector<PreviewState> prepared;
    prepared.reserve(sprites.size());
    std::unordered_map<std::string,
                       std::pair<std::shared_ptr<sf::Shader>, bool>>
        shaders;
    for (const PreviewSprite& sprite : sprites) {
        if (sprite.parentIndex < -1 ||
            (sprite.parentIndex >= 0 &&
             static_cast<std::size_t>(sprite.parentIndex) >= prepared.size())) {
            throw std::invalid_argument(
                "Preview sprite parentIndex must reference an earlier sprite");
        }
        PreviewState state;
        state.data = sprite;
        sf::Transformable transform;
        transform.setPosition(sprite.position + sprite.translation);
        transform.setRotation(sf::degrees(sprite.rotation));
        transform.setScale(sprite.scale);
        transform.setOrigin(sprite.origin);
        state.transform = transform.getTransform();
        state.bounds = {{0.0f, 0.0f},
                        {std::abs(static_cast<float>(sprite.rect.size.x)),
                         std::abs(static_cast<float>(sprite.rect.size.y))}};
        if (!sprite.shaderPath.empty()) {
            auto [entry, inserted] = shaders.try_emplace(sprite.shaderPath);
            if (inserted) {
                try {
                    entry->second.first = ShaderManager::load(
                        sprite.shaderPath, sf::Shader::Type::Fragment);
                } catch (const std::exception& error) {
                    std::cerr << error.what() << '\n';
                    entry->second.second = true;
                }
            }
            state.shader = entry->second.first;
            state.shaderError = entry->second.second;
        }
        prepared.push_back(std::move(state));
    }
    previewSprites = std::move(prepared);
    previewSpritesInstalled = true;
}

void GameMapRendererImpl::setPreviewVisibility(
    const std::vector<bool>& visibility) {
    if (!previewOnly) {
        throw std::logic_error(
            "Preview visibility requires a preview renderer");
    }
    if (!previewSpritesInstalled) {
        throw std::logic_error("Preview visibility requires installed sprites");
    }
    if (visibility.size() != previewSprites.size()) {
        throw std::invalid_argument(
            "Preview visibility must match the preview sprite count");
    }
    for (std::size_t index = 0; index < visibility.size(); ++index) {
        previewSprites[index].data.visible = visibility[index];
    }
}

void GameMapRendererImpl::refreshPreviewVisibility() {
    for (PreviewState& sprite : previewSprites) {
        const int parent = sprite.data.parentIndex;
        sprite.visibleOnMap =
            sprite.data.visible &&
            (parent < 0 ||
             previewSprites[static_cast<std::size_t>(parent)].visibleOnMap) &&
            map.isSpriteVisibleOnMap(sprite.data.mapPosition, sprite.bounds,
                                     sprite.transform);
    }
}

void GameMapRendererImpl::drawPreviewLayer(sf::RenderTarget& target,
                                           const sf::RenderStates& states,
                                           const std::string& layer) {
    for (const PreviewState& entry : previewSprites) {
        const PreviewSprite& data = entry.data;
        if (!entry.visibleOnMap || !data.texture || data.layer != layer) {
            continue;
        }
        sf::Sprite sprite(*data.texture, data.rect);
        sf::RenderStates spriteStates = states;
        spriteStates.transform.combine(entry.transform);
        if (entry.shaderError) {
            sprite.setColor(sf::Color::Magenta);
            target.draw(sprite, spriteStates);
            continue;
        }
        const float hue = drawableHue(data.hue);
        if (entry.shader) {
            setSpriteShaderUniforms(*entry.shader, *data.texture, data.rect,
                                    0.0f);
            if (hue != 0.0f && drawSpriteShaderWithHue(
                                   target, states, *data.texture, data.rect,
                                   entry.transform, *entry.shader, hue, 255)) {
                continue;
            }
            spriteStates.shader = entry.shader.get();
        } else if (hue != 0.0f) {
            applyActorHueUniform(hue);
            spriteStates.shader = actorHueShader.get();
        }
        target.draw(sprite, spriteStates);
    }
}
