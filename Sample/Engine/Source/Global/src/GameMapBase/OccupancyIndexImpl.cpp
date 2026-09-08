#include "OccupancyIndexImpl.hpp"
#include "OccupancyPageImpl.hpp"
#include <algorithm>
#include <optional>
#include <unordered_set>
namespace ludork::global::game_map_base_impl {

std::size_t OccupancyIndexImpl::getSparseOccupancyPageCount() const {
    return sparseOccupancyPages_.size();
}

int OccupancyIndexImpl::getOccupancyPageCoordinate(int value) {
    return ludork::global::game_map_base_impl::pageCoordinate(
        value, OccupancyPageSize);
}

int OccupancyIndexImpl::getOccupancyPageOffset(int value) {
    return ludork::global::game_map_base_impl::pageOffset(value,
                                                          OccupancyPageSize);
}

OccupancyIndexImpl::IntPair OccupancyIndexImpl::getOccupancyPageKey(int x,
                                                                    int y) {
    return ludork::global::game_map_base_impl::pageKey(x, y, OccupancyPageSize);
}

std::size_t OccupancyIndexImpl::getOccupancyPageCellIndex(int x, int y) {
    return ludork::global::game_map_base_impl::pageCellIndex(x, y,
                                                             OccupancyPageSize);
}

void OccupancyIndexImpl::clearActorOccupancy() {
    occupancyMap_.clear();
    sparseOccupancyPages_.clear();
}

const std::vector<Actor*>* OccupancyIndexImpl::findActorsAtCell(
    int x, int y, const std::optional<sf::Vector2u>& worldSize) const {
    if (worldSize.has_value()) {
        const IntPair pageKey = getOccupancyPageKey(x, y);
        const std::size_t cellIndex = getOccupancyPageCellIndex(x, y);
        const auto pageIt = sparseOccupancyPages_.find(pageKey);
        if (pageIt == sparseOccupancyPages_.end()) {
            return nullptr;
        }
        const std::vector<Actor*>& actors = pageIt->second.cells[cellIndex];
        return actors.empty() ? nullptr : &actors;
    }
    const auto occupancyIt = occupancyMap_.find({x, y});
    return occupancyIt == occupancyMap_.end() ? nullptr : &occupancyIt->second;
}

std::vector<Actor*> OccupancyIndexImpl::getActorsInRangeImpl(
    int x, int y, int radius, const Actor* excludedActor,
    const std::optional<sf::Vector2u>& worldSize) const {
    std::vector<Actor*> result;
    std::unordered_set<Actor*> seen;
    const auto appendActors = [&](const std::vector<Actor*>& actors) {
        for (Actor* actor : actors) {
            if (actor == excludedActor || actor->isDestroyed() ||
                !actor->isVisibleInHierarchy()) {
                continue;
            }
            if (seen.insert(actor).second) {
                result.push_back(actor);
            }
        }
    };
    if (worldSize.has_value()) {
        for (int ix = x - radius; ix <= x + radius; ++ix) {
            std::optional<IntPair> currentPageKey;
            const SparseOccupancyPage* currentPage = nullptr;
            for (int iy = y - radius; iy <= y + radius; ++iy) {
                const IntPair pageKey = getOccupancyPageKey(ix, iy);
                if (!currentPageKey.has_value() || *currentPageKey != pageKey) {
                    currentPageKey = pageKey;
                    const auto pageIt = sparseOccupancyPages_.find(pageKey);
                    currentPage = pageIt == sparseOccupancyPages_.end()
                                      ? nullptr
                                      : &pageIt->second;
                }
                if (currentPage == nullptr) {
                    continue;
                }
                const std::vector<Actor*>& actors =
                    currentPage->cells[getOccupancyPageCellIndex(ix, iy)];
                appendActors(actors);
            }
        }
        return result;
    }
    for (int ix = x - radius; ix <= x + radius; ++ix) {
        for (int iy = y - radius; iy <= y + radius; ++iy) {
            auto it = occupancyMap_.find({ix, iy});
            if (it == occupancyMap_.end()) {
                continue;
            }
            appendActors(it->second);
        }
    }
    return result;
}

void OccupancyIndexImpl::registerActorOccupancy(
    Actor& actor, const std::optional<sf::Vector2u>& worldSize) {
    const std::vector<sf::Vector2i> cells = actor.getOccupiedMapCells();
    registeredOccupancyCells_[&actor] = cells;
    for (const sf::Vector2i& cell : cells) {
        if (worldSize.has_value()) {
            const IntPair pageKey = getOccupancyPageKey(cell.x, cell.y);
            const std::size_t cellIndex =
                getOccupancyPageCellIndex(cell.x, cell.y);
            SparseOccupancyPage& page = sparseOccupancyPages_[pageKey];
            std::vector<Actor*>& actorsAtCell = page.cells[cellIndex];
            if (std::find(actorsAtCell.begin(), actorsAtCell.end(), &actor) ==
                actorsAtCell.end()) {
                if (actorsAtCell.empty()) {
                    ++page.occupiedCellCount;
                }
                actorsAtCell.push_back(&actor);
            }
            continue;
        }
        auto key = std::make_pair(cell.x, cell.y);
        auto& actorsAtCell = occupancyMap_[key];
        if (std::find(actorsAtCell.begin(), actorsAtCell.end(), &actor) ==
            actorsAtCell.end()) {
            actorsAtCell.push_back(&actor);
        }
    }
}

void OccupancyIndexImpl::unregisterActorOccupancy(
    Actor& actor, const std::optional<sf::Vector2u>& worldSize) {
    auto registeredIt = registeredOccupancyCells_.find(&actor);
    if (registeredIt != registeredOccupancyCells_.end()) {
        for (const sf::Vector2i& cell : registeredIt->second) {
            if (worldSize.has_value()) {
                const IntPair pageKey = getOccupancyPageKey(cell.x, cell.y);
                auto pageIt = sparseOccupancyPages_.find(pageKey);
                if (pageIt == sparseOccupancyPages_.end()) {
                    continue;
                }
                SparseOccupancyPage& page = pageIt->second;
                std::vector<Actor*>& actorsAtCell =
                    page.cells[getOccupancyPageCellIndex(cell.x, cell.y)];
                const bool wasOccupied = !actorsAtCell.empty();
                actorsAtCell.erase(std::remove(actorsAtCell.begin(),
                                               actorsAtCell.end(), &actor),
                                   actorsAtCell.end());
                if (wasOccupied && actorsAtCell.empty()) {
                    --page.occupiedCellCount;
                    if (page.occupiedCellCount == 0) {
                        sparseOccupancyPages_.erase(pageIt);
                    }
                }
                continue;
            }
            auto key = std::make_pair(cell.x, cell.y);
            auto occupancyIt = occupancyMap_.find(key);
            if (occupancyIt == occupancyMap_.end()) {
                continue;
            }
            auto& actorsAtCell = occupancyIt->second;
            actorsAtCell.erase(
                std::remove(actorsAtCell.begin(), actorsAtCell.end(), &actor),
                actorsAtCell.end());
            if (actorsAtCell.empty()) {
                occupancyMap_.erase(occupancyIt);
            }
        }
        registeredOccupancyCells_.erase(registeredIt);
        return;
    }
    if (worldSize.has_value()) {
        for (auto pageIt = sparseOccupancyPages_.begin();
             pageIt != sparseOccupancyPages_.end();) {
            SparseOccupancyPage& page = pageIt->second;
            for (std::vector<Actor*>& actorsAtCell : page.cells) {
                const bool wasOccupied = !actorsAtCell.empty();
                actorsAtCell.erase(std::remove(actorsAtCell.begin(),
                                               actorsAtCell.end(), &actor),
                                   actorsAtCell.end());
                if (wasOccupied && actorsAtCell.empty()) {
                    --page.occupiedCellCount;
                }
            }
            if (page.occupiedCellCount == 0) {
                pageIt = sparseOccupancyPages_.erase(pageIt);
            } else {
                ++pageIt;
            }
        }
        return;
    }
    for (auto it = occupancyMap_.begin(); it != occupancyMap_.end();) {
        auto& actorsAtCell = it->second;
        actorsAtCell.erase(
            std::remove(actorsAtCell.begin(), actorsAtCell.end(), &actor),
            actorsAtCell.end());
        if (actorsAtCell.empty()) {
            it = occupancyMap_.erase(it);
        } else {
            ++it;
        }
    }
}

void OccupancyIndexImpl::clearRegisteredCells() {
    registeredOccupancyCells_.clear();
}

const std::vector<sf::Vector2i>* OccupancyIndexImpl::registeredCells(
    Actor* actor) const {
    const auto iterator = registeredOccupancyCells_.find(actor);
    return iterator == registeredOccupancyCells_.end() ? nullptr
                                                       : &iterator->second;
}
}  // namespace ludork::global::game_map_base_impl
