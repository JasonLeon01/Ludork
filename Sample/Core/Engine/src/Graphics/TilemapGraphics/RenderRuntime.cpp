#include "RenderRuntime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ludork::engine::tilemap_graphics_impl {

LocalViewBounds localViewBounds(sf::RenderTarget& target,
                                const sf::Transform& transform) {
    const sf::View& view = target.getView();
    const sf::FloatRect viewport = view.getViewport();
    const sf::Vector2u targetSize = target.getSize();
    const int left = static_cast<int>(
        std::floor(viewport.position.x * static_cast<float>(targetSize.x)));
    const int top = static_cast<int>(
        std::floor(viewport.position.y * static_cast<float>(targetSize.y)));
    const int right =
        static_cast<int>(std::ceil((viewport.position.x + viewport.size.x) *
                                   static_cast<float>(targetSize.x)));
    const int bottom =
        static_cast<int>(std::ceil((viewport.position.y + viewport.size.y) *
                                   static_cast<float>(targetSize.y)));
    const std::array<sf::Vector2i, 4> pixelCorners = {
        sf::Vector2i(left, top), sf::Vector2i(right, top),
        sf::Vector2i(left, bottom), sf::Vector2i(right, bottom)};
    const sf::Transform inverse = transform.getInverse();
    LocalViewBounds bounds{std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::lowest(),
                           std::numeric_limits<float>::lowest()};
    for (const sf::Vector2i& pixel : pixelCorners) {
        const sf::Vector2f world = target.mapPixelToCoords(pixel, view);
        const sf::Vector2f local = inverse.transformPoint(world);
        bounds.left = std::min(bounds.left, local.x);
        bounds.top = std::min(bounds.top, local.y);
        bounds.right = std::max(bounds.right, local.x);
        bounds.bottom = std::max(bounds.bottom, local.y);
    }
    return bounds;
}

ChunkRange visibleChunkRange(const LocalViewBounds& visible,
                             const sf::Vector2f& layerSize, int tileSize,
                             int chunkSize, int chunkColumns, int chunkRows) {
    const float layerWidth = layerSize.x * static_cast<float>(tileSize);
    const float layerHeight = layerSize.y * static_cast<float>(tileSize);
    if (chunkColumns == 0 || chunkRows == 0 || tileSize <= 0 ||
        visible.right <= 0.0f || visible.bottom <= 0.0f ||
        visible.left >= layerWidth || visible.top >= layerHeight) {
        return {};
    }
    const float chunkPixelSize = static_cast<float>(chunkSize * tileSize);
    ChunkRange range;
    range.firstX =
        std::clamp(static_cast<int>(std::floor(visible.left / chunkPixelSize)),
                   0, chunkColumns - 1);
    range.firstY =
        std::clamp(static_cast<int>(std::floor(visible.top / chunkPixelSize)),
                   0, chunkRows - 1);
    range.lastX = std::clamp(
        static_cast<int>(std::ceil(visible.right / chunkPixelSize)) - 1, 0,
        chunkColumns - 1);
    range.lastY = std::clamp(
        static_cast<int>(std::ceil(visible.bottom / chunkPixelSize)) - 1, 0,
        chunkRows - 1);
    return range;
}

}  // namespace ludork::engine::tilemap_graphics_impl
