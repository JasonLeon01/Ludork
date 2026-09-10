#pragma once

#include "GridPointHash.hpp"
#include <Gameplay/TileLayer.hpp>
#include <Gameplay/Tilemap/Tilemap.hpp>
#include <General/Material.hpp>
#include <optional>
#include <unordered_map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ludork::global::game_map_base_impl {

class ActorRegistryImpl;
class SparseWorldImpl {
public:
    struct SparseWorldRegion {
        sf::IntRect rect;
        std::shared_ptr<Tilemap> tilemap;
        std::vector<std::shared_ptr<TileLayer>> layersTopFirst;
        bool actorsReady = false;
    };
    const std::optional<sf::Vector2u>& size() const;
    const std::vector<std::string>& layerOrder() const;
    const std::vector<SparseWorldRegion>& regions() const;
    bool isSparseWorldDirectionPassable(const sf::Vector2i& fromPosition,
                                        const sf::Vector2i& toPosition,
                                        int direction) const;
    void configureSparseWorld(const sf::Vector2u& size,
                              const std::vector<std::string>& layerOrder,
                              const std::vector<sf::IntRect>& regionRects);
    void setSparseWorldRegion(int regionIndex, std::shared_ptr<Tilemap> tilemap,
                              bool actorsReady);
    void setSparseWorldRegionActorsReady(int regionIndex);
    bool detachSparseWorldRegion(int regionIndex);
    void setSparseWorldPreparedRect(std::optional<sf::IntRect> rect);
    bool isSparseWorldCellReady(const sf::Vector2i& position) const;
    bool isSparseWorldGameplayPositionReady(const sf::Vector2i& position) const;
    std::optional<int> getSparseWorldRegionIndexAt(
        const sf::Vector2i& position) const;
    void clearSparseWorld();
    bool isSparseWorldTilePassable(const sf::Vector2i& position) const;
    std::optional<Material> getSparseWorldTopMaterial(
        const sf::Vector2i& position, const ActorRegistryImpl& actors) const;

private:
    using IntPair = std::pair<int, int>;
    using SparseWorldRegionPageMap =
        std::unordered_map<IntPair, std::vector<std::size_t>, GridPointHash>;
    const SparseWorldRegion* findSparseWorldRegion(
        const sf::Vector2i& position) const;
    SparseWorldRegion& requireSparseWorldRegion(int regionIndex);
    std::optional<sf::Vector2u> sparseWorldSize_;
    std::vector<std::string> sparseWorldLayerOrder_;
    std::vector<SparseWorldRegion> sparseWorldRegions_;
    SparseWorldRegionPageMap sparseWorldRegionPages_;
    std::optional<sf::IntRect> sparseWorldPreparedRect_;
};
}  // namespace ludork::global::game_map_base_impl
