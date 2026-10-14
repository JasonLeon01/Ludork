#pragma once

#include "MaterialPreviewSession.hpp"

#include <Graphics/MapRenderer.hpp>
#include <Runtime/AssetStore.hpp>

#include <optional>
#include <string>

namespace ludork::preview_host {

struct MaterialPreviewSession::Impl {
    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);
    std::shared_ptr<sf::Texture> texture(const std::string& path);
    std::shared_ptr<Tilemap> tilemap(sf::Vector2u size);

    MapRenderer renderer;
    std::shared_ptr<sf::Texture> source;
    std::shared_ptr<sf::Texture> empty;
    std::shared_ptr<sf::Texture> background;
    std::shared_ptr<Tilemap> map;
    sf::Vector2u mapSize;
    std::string sourcePath;
    std::optional<ludork::runtime::AssetStore::AssetStat> sourceStamp;
};

}  // namespace ludork::preview_host
