#include "LightOcclusionImpl.hpp"
#include "LightOcclusionMaskImpl.hpp"
#include "OccupancyIndexImpl.hpp"
#include "SparseWorldImpl.hpp"
#include <Gameplay/Actor.hpp>
#include <SFML/Graphics/Image.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
namespace ludork::global::game_map_base_impl {

void LightOcclusionImpl::clearStaticLightOccupancy() {
    staticLightOccupancy_.reset();
    staticLightOccupancyOrigin_ = {};
    staticLightOccupancySize_ = {};
    staticLightOccupancyPrefix_.clear();
}

std::shared_ptr<sf::Texture> LightOcclusionImpl::rebuildStaticLightOccupancy(
    const sf::Vector2i& origin, const sf::Vector2u& size,
    const std::vector<std::shared_ptr<Actor>>& actors,
    const SparseWorldImpl& world, const std::shared_ptr<Tilemap>& tilemap,
    const int& cellSize) {
    if (size.x == 0 || size.y == 0) {
        throw std::invalid_argument(
            "Static light occupancy size must be positive");
    }
    const std::size_t width = size.x;
    const std::size_t height = size.y;
    if (height > std::numeric_limits<std::size_t>::max() / width) {
        throw std::length_error("Static light occupancy size is too large");
    }
    const std::size_t cellCount = width * height;
    if (cellCount > std::numeric_limits<std::size_t>::max() / 4) {
        throw std::length_error("Static light occupancy image is too large");
    }

    std::vector<std::uint8_t> occupancy(cellCount, 0);
    const ludork::global::game_map_impl::CellBounds targetBounds =
        ludork::global::game_map_impl::cellBounds(origin, size);
    const auto markLayer = [&](TileLayer& layer,
                               const sf::Vector2i& layerOrigin) {
        if (!layer.getVisible()) {
            return;
        }
        const sf::Vector2u layerSize = layer.getGridSize();
        const ludork::global::game_map_impl::CellBounds overlap =
            ludork::global::game_map_impl::intersection(
                targetBounds, ludork::global::game_map_impl::cellBounds(
                                  layerOrigin, layerSize));
        if (ludork::global::game_map_impl::isEmpty(overlap)) {
            return;
        }
        const std::vector<std::vector<float>>& lightBlockMap =
            layer.getLightBlockMapView();
        for (std::int64_t worldY = overlap.top; worldY < overlap.bottom;
             ++worldY) {
            const std::size_t localY = static_cast<std::size_t>(
                worldY - static_cast<std::int64_t>(layerOrigin.y));
            const std::size_t occupancyY = static_cast<std::size_t>(
                worldY - static_cast<std::int64_t>(origin.y));
            const std::vector<float>& lightBlockRow = lightBlockMap[localY];
            for (std::int64_t worldX = overlap.left; worldX < overlap.right;
                 ++worldX) {
                const std::size_t localX = static_cast<std::size_t>(
                    worldX - static_cast<std::int64_t>(layerOrigin.x));
                if (lightBlockRow[localX] <= 0.0f) {
                    continue;
                }
                const std::size_t occupancyX = static_cast<std::size_t>(
                    worldX - static_cast<std::int64_t>(origin.x));
                occupancy[occupancyY * width + occupancyX] = 1;
            }
        }
    };

    if (world.size().has_value()) {
        for (const SparseWorldImpl::SparseWorldRegion& region :
             world.regions()) {
            if (!region.tilemap) {
                continue;
            }
            const ludork::global::game_map_impl::CellBounds overlap =
                ludork::global::game_map_impl::intersection(
                    targetBounds,
                    ludork::global::game_map_impl::cellBounds(region.rect));
            if (ludork::global::game_map_impl::isEmpty(overlap)) {
                continue;
            }
            for (const std::shared_ptr<TileLayer>& layer :
                 region.layersTopFirst) {
                markLayer(*layer, region.rect.position);
            }
        }
    } else if (tilemap) {
        for (const std::string& layerName : tilemap->getLayerNameList()) {
            const std::shared_ptr<TileLayer> layer =
                tilemap->getLayer(layerName);
            if (layer) {
                markLayer(*layer, {});
            }
        }
    }
    const double cellPixels = static_cast<double>(cellSize);
    for (const std::shared_ptr<Actor>& actor : actors) {
        if (!actor || actor->isDestroyed() || actor->getLightBlock() <= 0.0f) {
            continue;
        }
        const sf::FloatRect bounds = actor->getGlobalBounds();
        const ludork::global::game_map_impl::CellBounds actorCells{
            static_cast<std::int64_t>(std::floor(
                static_cast<double>(bounds.position.x) / cellPixels)),
            static_cast<std::int64_t>(std::floor(
                static_cast<double>(bounds.position.y) / cellPixels)),
            static_cast<std::int64_t>(std::ceil(
                static_cast<double>(bounds.position.x + bounds.size.x) /
                cellPixels)),
            static_cast<std::int64_t>(std::ceil(
                static_cast<double>(bounds.position.y + bounds.size.y) /
                cellPixels))};
        const ludork::global::game_map_impl::CellBounds overlap =
            ludork::global::game_map_impl::intersection(targetBounds,
                                                        actorCells);
        for (std::int64_t worldY = overlap.top; worldY < overlap.bottom;
             ++worldY) {
            const std::size_t occupancyY = static_cast<std::size_t>(
                worldY - static_cast<std::int64_t>(origin.y));
            for (std::int64_t worldX = overlap.left; worldX < overlap.right;
                 ++worldX) {
                const std::size_t occupancyX = static_cast<std::size_t>(
                    worldX - static_cast<std::int64_t>(origin.x));
                occupancy[occupancyY * width + occupancyX] = 1;
            }
        }
    }

    const std::size_t prefixWidth = width + 1;
    if (height + 1 > std::numeric_limits<std::size_t>::max() / prefixWidth) {
        throw std::length_error("Static light occupancy prefix is too large");
    }
    std::vector<std::size_t> prefix((height + 1) * prefixWidth, 0);
    std::vector<std::uint8_t> pixels(cellCount * 4, 0);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const std::size_t value = occupancy[y * width + x];
            const std::size_t prefixIndex = (y + 1) * prefixWidth + x + 1;
            prefix[prefixIndex] = value + prefix[prefixIndex - 1] +
                                  prefix[prefixIndex - prefixWidth] -
                                  prefix[prefixIndex - prefixWidth - 1];

            const std::size_t textureY = height - y - 1;
            const std::size_t pixelIndex = (textureY * width + x) * 4;
            const std::uint8_t channel = value == 0 ? 0 : 255;
            pixels[pixelIndex] = channel;
            pixels[pixelIndex + 1] = channel;
            pixels[pixelIndex + 2] = channel;
            pixels[pixelIndex + 3] = 255;
        }
    }

    sf::Image image(size, pixels.data());
    std::shared_ptr<sf::Texture> texture = std::make_shared<sf::Texture>();
    if (!texture->loadFromImage(image)) {
        throw std::runtime_error(
            "Failed to create the static light occupancy texture");
    }
    texture->setSmooth(false);

    staticLightOccupancy_ = texture;
    staticLightOccupancyOrigin_ = origin;
    staticLightOccupancySize_ = size;
    staticLightOccupancyPrefix_ = std::move(prefix);
    return texture;
}

bool LightOcclusionImpl::hasStaticLightOccupancy(const Light& light,
                                                 const int& cellSize) const {
    return ludork::global::game_map_impl::hasStaticOccupancy(
        staticLightOccupancyPrefix_, staticLightOccupancyOrigin_,
        staticLightOccupancySize_, light, cellSize);
}

std::vector<LightOcclusionResult> LightOcclusionImpl::analyseLightOcclusion(
    const std::vector<LightOcclusionInput>& inputs,
    const std::vector<std::shared_ptr<Actor>>& visibleActors,
    const OccupancyIndexImpl& occupancyIndex,
    const std::optional<sf::Vector2u>& worldSize, const int& cellSize) {
    std::unordered_map<Actor*, std::shared_ptr<Actor>> visibleActorOwners;
    visibleActorOwners.reserve(visibleActors.size());
    for (const std::shared_ptr<Actor>& actor : visibleActors) {
        if (actor) {
            visibleActorOwners.try_emplace(actor.get(), actor);
        }
    }

    std::vector<LightOcclusionResult> results;
    results.reserve(inputs.size());
    dynamicLightOccupancies_.resize(inputs.size());
    for (std::size_t inputIndex = 0; inputIndex < inputs.size(); ++inputIndex) {
        const LightOcclusionInput& input = inputs[inputIndex];
        LightOcclusionResult result;
        result.hasStaticTransmissionLoss =
            hasStaticLightOccupancy(input.light, cellSize);
        const Light& light = input.light;
        if (light.radius <= 0.0f || !std::isfinite(light.position.x) ||
            !std::isfinite(light.position.y) || !std::isfinite(light.radius)) {
            results.push_back(std::move(result));
            continue;
        }

        const float cellPixels = static_cast<float>(cellSize);
        const float rawCellRadius =
            std::ceil((light.radius + cellSize) / cellPixels);
        const float rawMapX = std::floor(light.position.x / cellPixels);
        const float rawMapY = std::floor(light.position.y / cellPixels);
        if (rawCellRadius >
                static_cast<float>(std::numeric_limits<int>::max()) ||
            rawMapX < static_cast<float>(std::numeric_limits<int>::min()) ||
            rawMapX > static_cast<float>(std::numeric_limits<int>::max()) ||
            rawMapY < static_cast<float>(std::numeric_limits<int>::min()) ||
            rawMapY > static_cast<float>(std::numeric_limits<int>::max())) {
            results.push_back(std::move(result));
            continue;
        }
        const int cellRadius = static_cast<int>(rawCellRadius);
        const int mapX = static_cast<int>(rawMapX);
        const int mapY = static_cast<int>(rawMapY);
        const std::vector<Actor*> actors = occupancyIndex.getActorsInRangeImpl(
            mapX, mapY, cellRadius, input.owner.get(), worldSize);

        std::optional<sf::FloatRect> actorBounds;
        std::vector<sf::FloatRect> occluderBounds;
        for (Actor* actor : actors) {
            const auto visibleActor = visibleActorOwners.find(actor);
            if (visibleActor == visibleActorOwners.end() ||
                actor->isDestroyed() || actor->getLightBlock() <= 0.0f) {
                continue;
            }
            const sf::FloatRect bounds = actor->getGlobalBounds();
            if (!ludork::global::game_map_impl::actorIntersectsLight(bounds,
                                                                     light)) {
                continue;
            }
            result.occluders.push_back(visibleActor->second);
            occluderBounds.push_back(bounds);
            if (!actorBounds.has_value()) {
                actorBounds = bounds;
                continue;
            }
            actorBounds = ludork::global::game_map_impl::enclosingRect(
                *actorBounds, bounds);
        }

        result.maskRect = ludork::global::game_map_impl::dynamicMaskRect(
            light, actorBounds, 2.0f);
        if (result.maskRect.has_value()) {
            ludork::global::game_map_impl::DynamicOccupancyResult occupancy =
                ludork::global::game_map_impl::rebuildDynamicOccupancy(
                    *result.maskRect, occluderBounds,
                    dynamicLightOccupancies_[inputIndex], cellPixels);
            dynamicLightOccupancies_[inputIndex] = occupancy.texture;
            result.dynamicOccupancy = std::move(occupancy.texture);
            result.dynamicOccupancyOrigin = occupancy.origin;
            result.dynamicOccupancySize = occupancy.size;
        }
        results.push_back(std::move(result));
    }
    return results;
}

}  // namespace ludork::global::game_map_base_impl
