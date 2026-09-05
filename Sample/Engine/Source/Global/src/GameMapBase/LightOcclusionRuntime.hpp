#pragma once

#include <Light.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace ludork::global::game_map_impl {

struct CellBounds {
    std::int64_t left = 0;
    std::int64_t top = 0;
    std::int64_t right = 0;
    std::int64_t bottom = 0;
};

struct DynamicOccupancyResult {
    std::shared_ptr<sf::Texture> texture;
    sf::Vector2f origin;
    sf::Vector2f size;
};

[[nodiscard]] CellBounds cellBounds(const sf::Vector2i& origin,
                                    const sf::Vector2u& size);
[[nodiscard]] CellBounds cellBounds(const sf::IntRect& rect);
[[nodiscard]] CellBounds intersection(const CellBounds& left,
                                      const CellBounds& right);
[[nodiscard]] bool isEmpty(const CellBounds& bounds);
[[nodiscard]] bool actorIntersectsLight(const sf::FloatRect& bounds,
                                        const Light& light);
[[nodiscard]] sf::FloatRect enclosingRect(const sf::FloatRect& left,
                                          const sf::FloatRect& right);
[[nodiscard]] std::optional<sf::FloatRect> dynamicMaskRect(
    const Light& light, const std::optional<sf::FloatRect>& actorBounds,
    float padding);
[[nodiscard]] bool hasStaticOccupancy(const std::vector<std::size_t>& prefix,
                                      const sf::Vector2i& origin,
                                      const sf::Vector2u& size,
                                      const Light& light, int cellSize);
[[nodiscard]] DynamicOccupancyResult rebuildDynamicOccupancy(
    const sf::FloatRect& maskRect,
    const std::vector<sf::FloatRect>& occluderBounds,
    std::shared_ptr<sf::Texture> texture, float cellSize);

}  // namespace ludork::global::game_map_impl
