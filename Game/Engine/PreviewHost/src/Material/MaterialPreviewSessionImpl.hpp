#pragma once

#include "MaterialPreviewSession.hpp"

#include <Graphics/MapRenderer.hpp>
#include <Graphics/Materials/MaterialResource.hpp>
#include <Runtime/AssetStore.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace ludork::preview_host {

struct MaterialPreviewSession::Impl {
    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);
    void prepare(const RuntimeData& asset, std::string path,
                 std::int64_t revision);
    void updateScene(const RuntimeData::Map& request, sf::Vector2u size);
    std::shared_ptr<sf::Texture> texture(const std::string& path);
    std::shared_ptr<Tilemap> tilemap(sf::Vector2u size);

    MapRenderer renderer;
    MapRenderer::Scene scene;
    MapRenderer::View view;
    RuntimeData sceneParameters{RuntimeData::Map{}};
    std::shared_ptr<const ludork::engine::materials::MaterialResource> material;
    std::int64_t materialRevision = -1;
    std::int64_t sceneMaterialRevision = -1;
    std::int64_t diagnosticsRevision = 0;
    std::string materialPath;
    std::string generatedVertex;
    std::string generatedFragment;
    RuntimeData::Array compileDiagnostics;
    RuntimeData diagnostics{RuntimeData::Array{}};
    std::shared_ptr<sf::Texture> source;
    std::shared_ptr<sf::Texture> empty;
    std::shared_ptr<sf::Texture> background;
    std::shared_ptr<Tilemap> map;
    sf::Vector2u mapSize;
    std::string sourcePath;
    std::optional<ludork::runtime::AssetStore::AssetStat> sourceStamp;
    std::optional<sf::Color> backgroundColour;

private:
    static constexpr char FrameResponse[] = "materialFrame";
};

}  // namespace ludork::preview_host
