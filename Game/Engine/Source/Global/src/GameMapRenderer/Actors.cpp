#include "GameMapRendererImpl.hpp"

#include <algorithm>
#include <stdexcept>

int GameMapRendererImpl::playerLayerIndex() const {
    const std::shared_ptr<Actor>& player = map.getPlayerActorForRenderer();
    if (!player || !player->isVisibleInHierarchy()) {
        return -1;
    }
    const ActorDict& actors = map.getMaterialActorsForRenderer();
    for (std::size_t index = 0; index < layerNames.size(); ++index) {
        const auto layerIt = actors.find(layerNames[index]);
        if (layerIt == actors.end()) {
            continue;
        }
        const auto playerIt =
            std::find_if(layerIt->second.begin(), layerIt->second.end(),
                         [&](const ActorPtr& actor) {
                             return actor.get() == player.get();
                         });
        if (playerIt != layerIt->second.end()) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

bool GameMapRendererImpl::preparePlayerCover(int layerIndex,
                                             int materialRevision,
                                             sf::Vector2i& playerPosition) {
    const std::shared_ptr<Actor>& player = map.getPlayerActorForRenderer();
    if (!player || !player->isVisibleInHierarchy() || layerIndex < 0) {
        resetTransparentTiles();
        return false;
    }
    playerPosition = player->getMapPosition();
    bool reusable = coverPlayerPosition == playerPosition &&
                    coverPlayerLayerIndex == layerIndex &&
                    coverMaterialRevision == materialRevision &&
                    coverLayers.size() == layerNames.size();
    if (reusable) {
        for (std::size_t index = 0; index < layerNames.size(); ++index) {
            const std::shared_ptr<TileLayer> layer =
                tilemap->getLayer(layerNames[index]);
            if (!layer || coverLayers[index].first != layer.get() ||
                coverLayers[index].second != layer->getVisible()) {
                reusable = false;
                break;
            }
        }
    }
    if (reusable) {
        return false;
    }

    resetTransparentTiles();
    coverPlayerPosition = playerPosition;
    coverPlayerLayerIndex = layerIndex;
    coverMaterialRevision = materialRevision;
    coverLayers.reserve(layerNames.size());
    for (const std::string& layerName : layerNames) {
        const std::shared_ptr<TileLayer> layer = tilemap->getLayer(layerName);
        if (!layer) {
            throw std::runtime_error("Game map layer is unavailable: " +
                                     layerName);
        }
        coverLayers.emplace_back(layer.get(), layer->getVisible());
    }
    return true;
}

void GameMapRendererImpl::resetTransparentTiles() {
    for (const TransparentTile& tile : transparentTiles) {
        if (tile.layer) {
            tile.layer->resetTileColor(tile.position.x, tile.position.y);
        }
    }
    transparentTiles.clear();
    coverLayers.clear();
    coverPlayerPosition.reset();
    coverPlayerLayerIndex = -1;
    coverMaterialRevision = -1;
}

void GameMapRendererImpl::applyPlayerCover(TileLayer& layer, int layerIndex,
                                           int playerLayerIndex,
                                           const sf::Vector2i& playerPosition) {
    if (layerIndex <= playerLayerIndex || playerLayerIndex < 0 ||
        !layer.hasContent(playerPosition)) {
        return;
    }
    const sf::Color colour{255, 255, 255, coverAlpha};
    for (const sf::Vector2i& position : layer.floodFillTransparent(
             playerPosition.x, playerPosition.y, colour)) {
        transparentTiles.push_back(
            {tilemap->getLayer(layer.getName()), position});
    }
}
