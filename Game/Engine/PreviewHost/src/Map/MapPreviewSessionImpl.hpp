#pragma once

#include "MapPreviewSession.hpp"
#include "MapEmitterPreviewImpl.hpp"

#include <Gameplay/TileLayer.hpp>
#include <Graphics/MapRenderer.hpp>
#include <Graphics/Materials/MaterialResource.hpp>
#include <SFML/Window/Context.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ludork::preview_host {

class MapPreviewSession::Impl {
public:
    static constexpr float DefaultAnimationInterval = 0.2f;
    ~Impl() noexcept;

    struct Layer {
        std::string signature;
        std::shared_ptr<TileLayer> graphics;
        TileGrid tiles;
        AutoTileGrid autoTiles;
    };

    struct Animation {
        std::size_t spriteIndex = 0;
        sf::IntRect baseRect;
        float interval = DefaultAnimationInterval;
        int frames = 1;
    };

    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);
    void activate();
    void clearScene(bool releaseEmitterResources = false);
    void reset();
    void updateScene(const RuntimeData::Map& data);
    std::shared_ptr<TileLayer> updateLayer(const std::string& cacheKey,
                                           const std::string& layerName,
                                           const RuntimeData::Map& layer,
                                           int width, int height,
                                           const RuntimeData::Map& tilesets,
                                           const RuntimeData::Map& autoTiles);
    std::shared_ptr<sf::Texture> texture(const std::string& path);
    std::uint64_t spriteId(const std::string& name);
    void updateSprite(std::size_t index, const RuntimeData::Map& fields);
    void updateLight(std::size_t index, const RuntimeData::Map& fields);
    void updateVisuals(const RuntimeData::Map& changes, const std::string& name,
                       bool updateGraphics);
    void updateSprites(const RuntimeData::Array& values);

    std::unique_ptr<sf::Context> context;
    MapRenderer renderer;
    MapEmitterPreviewImpl emitters;
    MapRenderer::Scene scene;
    RuntimeData::Map sceneData;
    std::unordered_map<std::string, Layer> layers;
    std::unordered_map<std::string, std::shared_ptr<sf::Texture>> textures;
    std::unique_ptr<ludork::engine::materials::MaterialResource::Overrides>
        materials;
    std::unordered_map<std::string, std::uint64_t> spriteIds;
    std::unordered_map<std::uint64_t, Animation> animations;
    std::uint64_t nextSpriteId = 1;
    std::string sceneKey;
    std::int64_t resourceRevision = -1;
    std::int64_t materialRevision = -1;
    bool hasScene = false;

private:
    static constexpr char FrameResponse[] = "mapFrame";
    static constexpr float MaximumParticleDeltaSeconds = 0.25f;
    static constexpr float DefaultLightRadius = 16.0f;
    static constexpr int AnimatedTileFrameCount = 3;
};

}  // namespace ludork::preview_host
