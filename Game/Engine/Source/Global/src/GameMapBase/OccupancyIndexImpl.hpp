#pragma once

#include "GridPointHash.hpp"
#include <Gameplay/Actor.hpp>
#include <array>
#include <unordered_map>
#include <vector>
#include <cstddef>
#include <optional>
#include <utility>

namespace ludork::global::game_map_base_impl {

class OccupancyIndexImpl {
public:
    using IntPair = std::pair<int, int>;
    static constexpr int OccupancyPageSize = 32;
    std::size_t getSparseOccupancyPageCount() const;
    static int getOccupancyPageCoordinate(int value);
    static int getOccupancyPageOffset(int value);
    static IntPair getOccupancyPageKey(int x, int y);
    static std::size_t getOccupancyPageCellIndex(int x, int y);
    void clearActorOccupancy();
    void clearRegisteredCells();
    const std::vector<sf::Vector2i>* registeredCells(Actor* actor) const;
    const std::vector<Actor*>* findActorsAtCell(
        int x, int y, const std::optional<sf::Vector2u>& worldSize) const;
    std::vector<Actor*> getActorsInRangeImpl(
        int x, int y, int radius, const Actor* excludedActor,
        const std::optional<sf::Vector2u>& worldSize) const;
    void registerActorOccupancy(Actor& actor,
                                const std::optional<sf::Vector2u>& worldSize);
    void unregisterActorOccupancy(Actor& actor,
                                  const std::optional<sf::Vector2u>& worldSize);

private:
    struct SparseOccupancyPage {
        std::array<std::vector<Actor*>, OccupancyPageSize * OccupancyPageSize>
            cells;
        std::size_t occupiedCellCount = 0;
    };
    std::unordered_map<IntPair, std::vector<Actor*>, GridPointHash>
        occupancyMap_;
    std::unordered_map<IntPair, SparseOccupancyPage, GridPointHash>
        sparseOccupancyPages_;
    std::unordered_map<Actor*, std::vector<sf::Vector2i>>
        registeredOccupancyCells_;
};
}  // namespace ludork::global::game_map_base_impl
