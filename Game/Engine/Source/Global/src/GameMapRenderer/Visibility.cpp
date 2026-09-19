#include "GameMapRendererImpl.hpp"

#include <utility>

void GameMapRendererImpl::prepareVisibleLayers() {
    const std::size_t revision = map.getVisibilityRevision();
    if (displayVisibilityRevision != revision) {
        resetTransparentTiles();
        if (map.getHideDisconnectedRegions()) {
            const auto sources = map.getDisplayTileSources();
            std::vector<std::shared_ptr<TileLayer>> layers;
            for (const std::string& name : layerNames) {
                if (const auto layer = sourceTilemap->getLayer(name)) {
                    layers.push_back(layer->createDisplayLayer(sources));
                }
            }
            tilemap = std::make_shared<Tilemap>(layers);
        } else {
            tilemap = sourceTilemap;
        }
        displayVisibilityRevision = revision;
        layerMaskTextures.clear();
        staticTransmissionRevision = -1;
        surfaceMaskRevision = -1;
        renderedLightingValid = false;
        cachedStaticMaterialRevision = -1;
        unobstructedLightCacheValid = false;
    }
    if (tilemap != sourceTilemap) {
        for (const std::string& name : layerNames) {
            const auto source = sourceTilemap->getLayer(name);
            const auto display = tilemap->getLayer(name);
            if (source && display) {
                display->syncDisplayAnimation(*source);
            }
        }
    }
}
