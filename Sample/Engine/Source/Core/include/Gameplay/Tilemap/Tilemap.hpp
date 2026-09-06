#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <General/TileLayerData.hpp>

class TileLayer;

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API Tilemap {
public:
    BIND_INIT()
    explicit Tilemap(const std::vector<std::shared_ptr<TileLayer>>& layers);
    virtual ~Tilemap();

    BIND_METHOD()
    void addLayer(const std::shared_ptr<TileLayer>& layer);

    BIND_METHOD(Pure = true, returns = "layer")
    virtual std::shared_ptr<TileLayer> getLayer(const std::string& name) const;

    BIND_METHOD(Pure = true, returns = "tiles")
    std::unordered_map<std::string, TileGrid> getTilesData() const;

    BIND_METHOD(Pure = true, returns = "layers")
    virtual std::unordered_map<std::string, std::shared_ptr<TileLayer>>
    getAllLayers() const;

    BIND_METHOD(Pure = true, returns = "layerNames")
    virtual std::vector<std::string> getLayerNameList() const;

    BIND_METHOD(Pure = true, returns = "autoTiles")
    std::unordered_map<std::string, AutoTileGrid> getAutoTilesData() const;

    BIND_METHOD(Pure = true, returns = "size")
    virtual sf::Vector2u getSize() const;

    BIND_METHOD()
    void updateAutoTileAnimation(float deltaTime, float frameInterval = 0.5f);

private:
    std::unordered_map<std::string, std::shared_ptr<TileLayer>> layers_;
    std::vector<std::string> layerNames_;
};
