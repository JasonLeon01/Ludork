#pragma once

#include "ChunkState.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/System/Vector2.hpp>

namespace ludork::engine::tilemap_graphics_impl {

struct LocalViewBounds {
    float left;
    float top;
    float right;
    float bottom;
};

LocalViewBounds localViewBounds(sf::RenderTarget& target,
                                const sf::Transform& transform);
ChunkRange visibleChunkRange(const LocalViewBounds& visible,
                             const sf::Vector2f& layerSize, int tileSize,
                             int chunkSize, int chunkColumns, int chunkRows);

}  // namespace ludork::engine::tilemap_graphics_impl
