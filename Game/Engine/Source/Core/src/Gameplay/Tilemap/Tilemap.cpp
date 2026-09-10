#include <Gameplay/Tilemap/Tilemap.hpp>
#include <Gameplay/TileLayer.hpp>

#include <stdexcept>

Tilemap::Tilemap(const std::vector<std::shared_ptr<TileLayer>>& layers) {
    for (const std::shared_ptr<TileLayer>& layer : layers) {
        addLayer(layer);
    }
}

Tilemap::~Tilemap() = default;

void Tilemap::addLayer(const std::shared_ptr<TileLayer>& layer) {
    if (layer == nullptr) {
        throw std::invalid_argument("Tilemap layer must not be null");
    }
    const std::string name = layer->getName();
    if (!layers_.contains(name)) {
        layerNames_.push_back(name);
    }
    layers_.insert_or_assign(name, layer);
}

std::shared_ptr<TileLayer> Tilemap::getLayer(const std::string& name) const {
    const auto iterator = layers_.find(name);
    return iterator == layers_.end() ? nullptr : iterator->second;
}

std::unordered_map<std::string, TileGrid> Tilemap::getTilesData() const {
    std::unordered_map<std::string, TileGrid> result;
    for (const std::string& name : layerNames_) {
        const std::shared_ptr<TileLayer> layer = getLayer(name);
        if (layer != nullptr) {
            result.emplace(name, layer->getTiles());
        }
    }
    return result;
}

std::unordered_map<std::string, std::shared_ptr<TileLayer>>
Tilemap::getAllLayers() const {
    return layers_;
}

std::vector<std::string> Tilemap::getLayerNameList() const {
    return layerNames_;
}

std::unordered_map<std::string, AutoTileGrid> Tilemap::getAutoTilesData()
    const {
    std::unordered_map<std::string, AutoTileGrid> result;
    for (const std::string& name : layerNames_) {
        const std::shared_ptr<TileLayer> layer = getLayer(name);
        if (layer != nullptr) {
            result.emplace(name, layer->getAutoTiles());
        }
    }
    return result;
}

sf::Vector2u Tilemap::getSize() const {
    if (layerNames_.empty()) {
        return {0, 0};
    }
    const std::shared_ptr<TileLayer> layer = getLayer(layerNames_.front());
    return layer == nullptr ? sf::Vector2u(0, 0) : layer->getGridSize();
}

void Tilemap::updateAutoTileAnimation(float deltaTime, float frameInterval) {
    for (const std::string& name : layerNames_) {
        const std::shared_ptr<TileLayer> layer = getLayer(name);
        if (layer == nullptr) {
            continue;
        }
        layer->updateAutoTileAnimation(deltaTime, frameInterval);
        layer->updateShader(deltaTime);
    }
}
