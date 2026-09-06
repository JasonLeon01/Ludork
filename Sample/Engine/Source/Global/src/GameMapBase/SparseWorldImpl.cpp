#include "SparseWorldImpl.hpp"
#include "ActorRegistryImpl.hpp"
#include "OccupancyIndexImpl.hpp"
#include "SparseWorldGeometryImpl.hpp"
#include <EngineState.hpp>
#include <limits>
#include <stdexcept>
#include <unordered_set>
namespace ludork::global::game_map_base_impl {

bool SparseWorldImpl::isSparseWorldDirectionPassable(
    const sf::Vector2i& fromPosition, const sf::Vector2i& toPosition,
    int direction) const {
    if (!isSparseWorldGameplayPositionReady(fromPosition) ||
        !isSparseWorldGameplayPositionReady(toPosition)) {
        return false;
    }
    const int opposite = oppositeDirection(direction);
    const SparseWorldRegion* fromRegion = findSparseWorldRegion(fromPosition);
    const SparseWorldRegion* toRegion = findSparseWorldRegion(toPosition);
    const sf::Vector2i fromLocal =
        fromRegion == nullptr ? sf::Vector2i{}
                              : fromPosition - fromRegion->rect.position;
    const sf::Vector2i toLocal = toRegion == nullptr
                                     ? sf::Vector2i{}
                                     : toPosition - toRegion->rect.position;
    bool fromFound = false;
    bool toFound = false;
    for (std::size_t index = 0; index < sparseWorldLayerOrder_.size();
         ++index) {
        if (!fromFound && fromRegion != nullptr) {
            const std::shared_ptr<TileLayer>& layer =
                fromRegion->layersTopFirst[index];
            if (layer->getVisible() && layer->hasContent(fromLocal)) {
                if (!layer->isDirectionPassable(fromLocal, direction)) {
                    return false;
                }
                fromFound = true;
            }
        }
        if (!toFound && toRegion != nullptr) {
            const std::shared_ptr<TileLayer>& layer =
                toRegion->layersTopFirst[index];
            if (layer->getVisible() && layer->hasContent(toLocal)) {
                if (!layer->isDirectionPassable(toLocal, opposite)) {
                    return false;
                }
                toFound = true;
            }
        }
        if (fromFound && toFound) {
            break;
        }
    }
    return true;
}

void SparseWorldImpl::configureSparseWorld(
    const sf::Vector2u& size, const std::vector<std::string>& layerOrder,
    const std::vector<sf::IntRect>& regionRects) {
    if (size.x > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
        size.y > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("Sparse world size is out of range");
    }

    std::unordered_set<std::string> uniqueLayerNames;
    uniqueLayerNames.reserve(layerOrder.size());
    for (const std::string& layerName : layerOrder) {
        if (!uniqueLayerNames.insert(layerName).second) {
            throw std::invalid_argument(
                "Sparse world layer order contains a duplicate name");
        }
    }

    std::vector<SparseWorldRegion> regions;
    regions.reserve(regionRects.size());
    SparseWorldRegionPageMap regionPages;
    for (const sf::IntRect& rect : regionRects) {
        const std::int64_t right =
            static_cast<std::int64_t>(rect.position.x) + rect.size.x;
        const std::int64_t bottom =
            static_cast<std::int64_t>(rect.position.y) + rect.size.y;
        if (rect.position.x < 0 || rect.position.y < 0 || rect.size.x <= 0 ||
            rect.size.y <= 0 || right > static_cast<std::int64_t>(size.x) ||
            bottom > static_cast<std::int64_t>(size.y)) {
            throw std::invalid_argument(
                "Sparse world region rectangle is outside the world");
        }
        for (const SparseWorldRegion& existing : regions) {
            if (rectsIntersect(existing.rect, rect)) {
                throw std::invalid_argument(
                    "Sparse world region rectangles must not overlap");
            }
        }
        regions.push_back({.rect = rect});
        const std::size_t regionIndex = regions.size() - 1;
        const IntPair firstPage = OccupancyIndexImpl::getOccupancyPageKey(
            rect.position.x, rect.position.y);
        const IntPair lastPage = OccupancyIndexImpl::getOccupancyPageKey(
            static_cast<int>(right - 1), static_cast<int>(bottom - 1));
        for (int pageY = firstPage.second; pageY <= lastPage.second; ++pageY) {
            for (int pageX = firstPage.first; pageX <= lastPage.first;
                 ++pageX) {
                regionPages[{pageX, pageY}].push_back(regionIndex);
            }
        }
    }

    sparseWorldSize_ = size;
    sparseWorldLayerOrder_ = layerOrder;
    sparseWorldRegions_ = std::move(regions);
    sparseWorldRegionPages_ = std::move(regionPages);
    sparseWorldPreparedRect_.reset();
}

void SparseWorldImpl::setSparseWorldRegion(int regionIndex,
                                           std::shared_ptr<Tilemap> tilemap,
                                           bool actorsReady) {
    if (!tilemap) {
        throw std::invalid_argument("Sparse world region Tilemap is null");
    }
    SparseWorldRegion& region = requireSparseWorldRegion(regionIndex);
    const sf::Vector2u expectedSize(
        static_cast<unsigned int>(region.rect.size.x),
        static_cast<unsigned int>(region.rect.size.y));
    if (tilemap->getSize() != expectedSize) {
        throw std::invalid_argument(
            "Sparse world region Tilemap size does not match its rectangle");
    }

    std::vector<std::shared_ptr<TileLayer>> layersTopFirst;
    layersTopFirst.reserve(sparseWorldLayerOrder_.size());
    for (auto layerName = sparseWorldLayerOrder_.rbegin();
         layerName != sparseWorldLayerOrder_.rend(); ++layerName) {
        std::shared_ptr<TileLayer> layer = tilemap->getLayer(*layerName);
        if (!layer) {
            throw std::invalid_argument(
                "Sparse world region Tilemap is missing a configured layer");
        }
        if (layer->getGridSize() != expectedSize) {
            throw std::invalid_argument(
                "Sparse world region layer size does not match its rectangle");
        }
        layersTopFirst.push_back(std::move(layer));
    }

    region.tilemap = std::move(tilemap);
    region.layersTopFirst = std::move(layersTopFirst);
    region.actorsReady = actorsReady;
}

void SparseWorldImpl::setSparseWorldRegionActorsReady(int regionIndex) {
    SparseWorldRegion& region = requireSparseWorldRegion(regionIndex);
    if (!region.tilemap) {
        throw std::logic_error(
            "Sparse world region Tilemap must be set before Actors are ready");
    }
    region.actorsReady = true;
}

bool SparseWorldImpl::detachSparseWorldRegion(int regionIndex) {
    SparseWorldRegion& region = requireSparseWorldRegion(regionIndex);
    if (!region.tilemap && region.layersTopFirst.empty() &&
        !region.actorsReady) {
        return false;
    }
    region.tilemap.reset();
    region.layersTopFirst.clear();
    region.actorsReady = false;

    return true;
}

void SparseWorldImpl::setSparseWorldPreparedRect(
    std::optional<sf::IntRect> rect) {
    if (!rect.has_value()) {
        sparseWorldPreparedRect_.reset();
        return;
    }
    if (!sparseWorldSize_.has_value()) {
        throw std::logic_error("Sparse world is not configured");
    }
    const std::int64_t right =
        static_cast<std::int64_t>(rect->position.x) + rect->size.x;
    const std::int64_t bottom =
        static_cast<std::int64_t>(rect->position.y) + rect->size.y;
    if (rect->position.x < 0 || rect->position.y < 0 || rect->size.x < 0 ||
        rect->size.y < 0 ||
        right > static_cast<std::int64_t>(sparseWorldSize_->x) ||
        bottom > static_cast<std::int64_t>(sparseWorldSize_->y)) {
        throw std::invalid_argument(
            "Sparse world prepared rectangle is outside the world");
    }
    sparseWorldPreparedRect_ = *rect;
}

bool SparseWorldImpl::isSparseWorldCellReady(
    const sf::Vector2i& position) const {
    if (!sparseWorldSize_.has_value() || position.x < 0 || position.y < 0 ||
        position.x >= static_cast<int>(sparseWorldSize_->x) ||
        position.y >= static_cast<int>(sparseWorldSize_->y)) {
        return false;
    }
    const SparseWorldRegion* region = findSparseWorldRegion(position);
    if (region == nullptr) {
        return true;
    }
    if (!region->tilemap || !region->actorsReady) {
        return false;
    }
    const sf::Vector2i localPosition = position - region->rect.position;
    for (const std::shared_ptr<TileLayer>& layer : region->layersTopFirst) {
        if (!layer->isCellBuilt(localPosition)) {
            return false;
        }
    }
    return true;
}

bool SparseWorldImpl::isSparseWorldGameplayPositionReady(
    const sf::Vector2i& position) const {
    return (!sparseWorldPreparedRect_.has_value() ||
            rectContains(*sparseWorldPreparedRect_, position)) &&
           isSparseWorldCellReady(position);
}

std::optional<int> SparseWorldImpl::getSparseWorldRegionIndexAt(
    const sf::Vector2i& position) const {
    const auto pageIt = sparseWorldRegionPages_.find(
        OccupancyIndexImpl::getOccupancyPageKey(position.x, position.y));
    if (pageIt == sparseWorldRegionPages_.end()) {
        return std::nullopt;
    }
    for (const std::size_t regionIndex : pageIt->second) {
        const SparseWorldRegion& region = sparseWorldRegions_[regionIndex];
        if (rectContains(region.rect, position)) {
            return static_cast<int>(regionIndex + 1);
        }
    }
    return std::nullopt;
}

void SparseWorldImpl::clearSparseWorld() {
    sparseWorldSize_.reset();
    sparseWorldLayerOrder_.clear();
    sparseWorldRegions_.clear();
    sparseWorldRegionPages_.clear();
    sparseWorldPreparedRect_.reset();
}

const SparseWorldImpl::SparseWorldRegion*
SparseWorldImpl::findSparseWorldRegion(const sf::Vector2i& position) const {
    const std::optional<int> regionIndex =
        getSparseWorldRegionIndexAt(position);
    if (!regionIndex.has_value()) {
        return nullptr;
    }
    return &sparseWorldRegions_[static_cast<std::size_t>(*regionIndex - 1)];
}

SparseWorldImpl::SparseWorldRegion& SparseWorldImpl::requireSparseWorldRegion(
    int regionIndex) {
    if (!sparseWorldSize_.has_value()) {
        throw std::logic_error("Sparse world is not configured");
    }
    if (regionIndex <= 0 ||
        regionIndex > static_cast<int>(sparseWorldRegions_.size())) {
        throw std::out_of_range("Sparse world region index is out of range");
    }
    return sparseWorldRegions_[static_cast<std::size_t>(regionIndex - 1)];
}

bool SparseWorldImpl::isSparseWorldTilePassable(
    const sf::Vector2i& position) const {
    if (!isSparseWorldGameplayPositionReady(position)) {
        return false;
    }
    const SparseWorldRegion* region = findSparseWorldRegion(position);
    if (region == nullptr) {
        return true;
    }
    const sf::Vector2i localPosition = position - region->rect.position;
    for (const std::shared_ptr<TileLayer>& layer : region->layersTopFirst) {
        if (!layer->getVisible() || !layer->hasContent(localPosition)) {
            continue;
        }
        return layer->isPassable(localPosition);
    }
    return true;
}

std::optional<Material> SparseWorldImpl::getSparseWorldTopMaterial(
    const sf::Vector2i& position, const ActorRegistryImpl& actors) const {
    if (!isSparseWorldCellReady(position)) {
        return std::nullopt;
    }
    const SparseWorldRegion* region = findSparseWorldRegion(position);
    if (region == nullptr) {
        return std::nullopt;
    }
    const sf::Vector2i localPosition = position - region->rect.position;
    for (std::size_t index = 0; index < sparseWorldLayerOrder_.size();
         ++index) {
        const std::string& layerName =
            sparseWorldLayerOrder_[sparseWorldLayerOrder_.size() - index - 1];
        const auto actorLayerIt = actors.materialActors().find(layerName);
        if (actorLayerIt != actors.materialActors().end()) {
            for (const std::shared_ptr<Actor>& actor : actorLayerIt->second) {
                if (actor && actor.get() != actors.playerActor().get() &&
                    !actor->isDestroyed() &&
                    actor->getMapPosition() == position) {
                    return actor->getMaterial();
                }
            }
        }
        const std::shared_ptr<TileLayer>& layer = region->layersTopFirst[index];
        if (!layer->getVisible()) {
            continue;
        }
        const std::optional<Material> material =
            layer->getMaterial(localPosition);
        if (material.has_value()) {
            return material;
        }
    }
    return std::nullopt;
}

const std::optional<sf::Vector2u>& SparseWorldImpl::size() const {
    return sparseWorldSize_;
}
const std::vector<std::string>& SparseWorldImpl::layerOrder() const {
    return sparseWorldLayerOrder_;
}
const std::vector<SparseWorldImpl::SparseWorldRegion>&
SparseWorldImpl::regions() const {
    return sparseWorldRegions_;
}
}  // namespace ludork::global::game_map_base_impl
