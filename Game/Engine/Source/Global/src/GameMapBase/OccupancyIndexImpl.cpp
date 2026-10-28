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
    registeredOccupancyCells_.clear();
    activeActors_.clear();
    actorOrder_.clear();
}

void OccupancyIndexImpl::syncActors(
    const std::vector<Actor*>& orderedActors,
    const std::optional<sf::Vector2u>& worldSize) {
    std::vector<Actor*> nextActors;
    std::unordered_map<Actor*, std::size_t> nextOrder;
    nextActors.reserve(orderedActors.size());
    nextOrder.reserve(orderedActors.size());
    for (Actor* actor : orderedActors) {
        if (actor != nullptr &&
            nextOrder.try_emplace(actor, nextActors.size()).second) {
            nextActors.push_back(actor);
        }
    }

    std::vector<Actor*> previousRetainedActors;
    std::vector<Actor*> nextRetainedActors;
    previousRetainedActors.reserve(activeActors_.size());
    nextRetainedActors.reserve(nextActors.size());
    for (Actor* actor : activeActors_) {
        if (nextOrder.contains(actor)) {
            previousRetainedActors.push_back(actor);
        } else {
            unregisterActorOccupancy(*actor, worldSize);
        }
    }
    for (Actor* actor : nextActors) {
        if (actorOrder_.contains(actor)) {
            nextRetainedActors.push_back(actor);
        }
    }

    std::vector<Actor*> reorderedActors;
    for (std::size_t index = 0; index < previousRetainedActors.size();
         ++index) {
        if (previousRetainedActors[index] != nextRetainedActors[index]) {
            reorderedActors.push_back(previousRetainedActors[index]);
        }
    }
    activeActors_ = std::move(nextActors);
    actorOrder_ = std::move(nextOrder);
    reorderActorCells(reorderedActors, worldSize);
    refreshActors(worldSize);
}

void OccupancyIndexImpl::refreshActors(
    const std::optional<sf::Vector2u>& worldSize) {
    for (Actor* actor : activeActors_) {
        updateActorOccupancy(*actor, worldSize);
    }
}

void OccupancyIndexImpl::updateActorOccupancy(
    Actor& actor, const std::optional<sf::Vector2u>& worldSize) {
    if (!actorOrder_.contains(&actor)) {
        return;
    }
    if (actor.isDestroyed()) {
        unregisterActorOccupancy(actor, worldSize);
        return;
    }
    std::vector<sf::Vector2i> cells = actor.getOccupiedMapCells();
    const std::vector<sf::Vector2i>* registered = registeredCells(&actor);
    if (registered != nullptr && *registered == cells) {
        return;
    }
    unregisterActorOccupancy(actor, worldSize);
    registerActorOccupancy(actor, std::move(cells), worldSize);
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
    Actor& actor, std::vector<sf::Vector2i> cells,
    const std::optional<sf::Vector2u>& worldSize) {
    const auto registered =
        registeredOccupancyCells_.emplace(&actor, std::move(cells)).first;
    for (const sf::Vector2i& cell : registered->second) {
        if (worldSize.has_value()) {
            const IntPair pageKey = getOccupancyPageKey(cell.x, cell.y);
            const std::size_t cellIndex =
                getOccupancyPageCellIndex(cell.x, cell.y);
            SparseOccupancyPage& page = sparseOccupancyPages_[pageKey];
            std::vector<Actor*>& actorsAtCell = page.cells[cellIndex];
            if (actorsAtCell.empty()) {
                ++page.occupiedCellCount;
            }
            insertActorInCell(actorsAtCell, actor);
            continue;
        }
        auto key = std::make_pair(cell.x, cell.y);
        auto& actorsAtCell = occupancyMap_[key];
        insertActorInCell(actorsAtCell, actor);
    }
}

void OccupancyIndexImpl::insertActorInCell(std::vector<Actor*>& actors,
                                           Actor& actor) {
    const std::size_t order = actorOrder_.at(&actor);
    const auto position =
        std::lower_bound(actors.begin(), actors.end(), order,
                         [&](Actor* existing, std::size_t value) {
                             return actorOrder_.at(existing) < value;
                         });
    if (position == actors.end() || *position != &actor) {
        actors.insert(position, &actor);
    }
}

void OccupancyIndexImpl::reorderActorCells(
    const std::vector<Actor*>& actors,
    const std::optional<sf::Vector2u>& worldSize) {
    std::unordered_set<IntPair, GridPointHash> reorderedCells;
    for (Actor* actor : actors) {
        const std::vector<sf::Vector2i>* cells = registeredCells(actor);
        if (cells == nullptr) {
            continue;
        }
        for (const sf::Vector2i& cell : *cells) {
            if (!reorderedCells.emplace(cell.x, cell.y).second) {
                continue;
            }
            std::vector<Actor*>* actorsAtCell = nullptr;
            if (worldSize.has_value()) {
                const auto page = sparseOccupancyPages_.find(
                    getOccupancyPageKey(cell.x, cell.y));
                if (page != sparseOccupancyPages_.end()) {
                    actorsAtCell =
                        &page->second
                             .cells[getOccupancyPageCellIndex(cell.x, cell.y)];
                }
            } else {
                const auto entry = occupancyMap_.find({cell.x, cell.y});
                if (entry != occupancyMap_.end()) {
                    actorsAtCell = &entry->second;
                }
            }
            if (actorsAtCell != nullptr) {
                std::sort(actorsAtCell->begin(), actorsAtCell->end(),
                          [&](Actor* left, Actor* right) {
                              return actorOrder_.at(left) <
                                     actorOrder_.at(right);
                          });
            }
        }
    }
}

void OccupancyIndexImpl::unregisterActorOccupancy(
    Actor& actor, const std::optional<sf::Vector2u>& worldSize) {
    auto registeredIt = registeredOccupancyCells_.find(&actor);
    if (registeredIt == registeredOccupancyCells_.end()) {
        return;
    }
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
            actorsAtCell.erase(
                std::remove(actorsAtCell.begin(), actorsAtCell.end(), &actor),
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
}

const std::vector<sf::Vector2i>* OccupancyIndexImpl::registeredCells(
    Actor* actor) const {
    const auto iterator = registeredOccupancyCells_.find(actor);
    return iterator == registeredOccupancyCells_.end() ? nullptr
                                                       : &iterator->second;
}
}  // namespace ludork::global::game_map_base_impl
