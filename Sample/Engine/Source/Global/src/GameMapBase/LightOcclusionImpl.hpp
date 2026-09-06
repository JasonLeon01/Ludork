#pragma once

#include <Light.hpp>
#include <LightOcclusionInput.hpp>
#include <LightOcclusionResult.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <memory>
#include <vector>
class Tilemap;
#include <cstddef>
#include <optional>

namespace ludork::global::game_map_base_impl {

class OccupancyIndexImpl;
class SparseWorldImpl;
class LightOcclusionImpl {
public:
    void clearStaticLightOccupancy();
    std::shared_ptr<sf::Texture> rebuildStaticLightOccupancy(
        const sf::Vector2i& origin, const sf::Vector2u& size,
        const std::vector<std::shared_ptr<Actor>>& actors,
        const SparseWorldImpl& world, const std::shared_ptr<Tilemap>& tilemap,
        const int& cellSize);
    std::vector<LightOcclusionResult> analyseLightOcclusion(
        const std::vector<LightOcclusionInput>& inputs,
        const std::vector<std::shared_ptr<Actor>>& visibleActors,
        const OccupancyIndexImpl& occupancyIndex,
        const std::optional<sf::Vector2u>& worldSize, const int& cellSize);

private:
    bool hasStaticLightOccupancy(const Light& light, const int& cellSize) const;
    std::shared_ptr<sf::Texture> staticLightOccupancy_;
    sf::Vector2i staticLightOccupancyOrigin_;
    sf::Vector2u staticLightOccupancySize_;
    std::vector<std::size_t> staticLightOccupancyPrefix_;
    std::vector<std::shared_ptr<sf::Texture>> dynamicLightOccupancies_;
};
}  // namespace ludork::global::game_map_base_impl
