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
        state.materialResource =
            ludork::engine::materials::MaterialResource::load(sprite.material);
        state.materialResource->validateParameters(sprite.materialParameters);
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
