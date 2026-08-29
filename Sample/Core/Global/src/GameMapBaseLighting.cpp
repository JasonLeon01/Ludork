#include "GameMapBase.hpp"

#include <Runtime/EngineState.hpp>

#include <SFML/Graphics/Image.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr float kDynamicTransmissionPadding = 2.0f;

struct CellBounds {
    std::int64_t left = 0;
    std::int64_t top = 0;
    std::int64_t right = 0;
    std::int64_t bottom = 0;
};

CellBounds cellBounds(const sf::Vector2i& origin, const sf::Vector2u& size) {
    return {origin.x, origin.y, static_cast<std::int64_t>(origin.x) + size.x,
            static_cast<std::int64_t>(origin.y) + size.y};
}

CellBounds cellBounds(const sf::IntRect& rect) {
    return {rect.position.x, rect.position.y,
            static_cast<std::int64_t>(rect.position.x) + rect.size.x,
            static_cast<std::int64_t>(rect.position.y) + rect.size.y};
}

CellBounds intersection(const CellBounds& left, const CellBounds& right) {
    return {std::max(left.left, right.left), std::max(left.top, right.top),
            std::min(left.right, right.right),
            std::min(left.bottom, right.bottom)};
}

bool isEmpty(const CellBounds& bounds) {
    return bounds.left >= bounds.right || bounds.top >= bounds.bottom;
}

bool actorIntersectsLight(const sf::FloatRect& bounds, const Light& light) {
    return bounds.position.x <= light.position.x + light.radius &&
           bounds.position.x + bounds.size.x >=
               light.position.x - light.radius &&
           bounds.position.y <= light.position.y + light.radius &&
           bounds.position.y + bounds.size.y >= light.position.y - light.radius;
}
}  // namespace

void GameMapBase::clearStaticLightOccupancy() {
    staticLightOccupancy_.reset();
    staticLightOccupancyOrigin_ = {};
    staticLightOccupancySize_ = {};
    staticLightOccupancyPrefix_.clear();
}

std::shared_ptr<sf::Texture> GameMapBase::rebuildStaticLightOccupancy(
    const sf::Vector2i& origin, const sf::Vector2u& size) {
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
    const CellBounds targetBounds = cellBounds(origin, size);
    const auto markLayer = [&](TileLayer& layer,
                               const sf::Vector2i& layerOrigin) {
        if (!layer.getVisible()) {
            return;
        }
        const sf::Vector2u layerSize = layer.getGridSize();
        const CellBounds overlap =
            intersection(targetBounds, cellBounds(layerOrigin, layerSize));
        if (isEmpty(overlap)) {
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

    if (sparseWorldSize_.has_value()) {
        for (SparseWorldRegion& region : sparseWorldRegions_) {
            if (!region.tilemap) {
                continue;
            }
            const CellBounds overlap =
                intersection(targetBounds, cellBounds(region.rect));
            if (isEmpty(overlap)) {
                continue;
            }
            for (const std::shared_ptr<TileLayer>& layer :
                 region.layersTopFirst) {
                markLayer(*layer, region.rect.position);
            }
        }
    } else if (tilemap_) {
        for (const std::string& layerName : tilemap_->getLayerNameList()) {
            const std::shared_ptr<TileLayer> layer =
                tilemap_->getLayer(layerName);
            if (layer) {
                markLayer(*layer, {});
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

bool GameMapBase::hasStaticLightOccupancy(const Light& light) const {
    if (staticLightOccupancyPrefix_.empty() ||
        staticLightOccupancySize_.x == 0 || staticLightOccupancySize_.y == 0 ||
        light.radius <= 0.0f || !std::isfinite(light.position.x) ||
        !std::isfinite(light.position.y) || !std::isfinite(light.radius)) {
        return false;
    }
    const double cellSize = static_cast<double>(CellSize);
    const double minimumXValue =
        std::floor((static_cast<double>(light.position.x) - light.radius) /
                   cellSize) -
        1.0;
    const double minimumYValue =
        std::floor((static_cast<double>(light.position.y) - light.radius) /
                   cellSize) -
        1.0;
    const double maximumXValue =
        std::floor((static_cast<double>(light.position.x) + light.radius) /
                   cellSize) +
        1.0;
    const double maximumYValue =
        std::floor((static_cast<double>(light.position.y) + light.radius) /
                   cellSize) +
        1.0;
    constexpr double MinimumCoordinate = -4611686018427387904.0;
    constexpr double MaximumCoordinate = 4611686018427387904.0;
    if (minimumXValue < MinimumCoordinate ||
        minimumYValue < MinimumCoordinate ||
        maximumXValue > MaximumCoordinate ||
        maximumYValue > MaximumCoordinate) {
        return false;
    }
    std::int64_t minimumX = static_cast<std::int64_t>(minimumXValue);
    std::int64_t minimumY = static_cast<std::int64_t>(minimumYValue);
    std::int64_t maximumX = static_cast<std::int64_t>(maximumXValue);
    std::int64_t maximumY = static_cast<std::int64_t>(maximumYValue);
    const std::int64_t occupancyRight =
        static_cast<std::int64_t>(staticLightOccupancyOrigin_.x) +
        staticLightOccupancySize_.x;
    const std::int64_t occupancyBottom =
        static_cast<std::int64_t>(staticLightOccupancyOrigin_.y) +
        staticLightOccupancySize_.y;
    minimumX = std::max(
        minimumX, static_cast<std::int64_t>(staticLightOccupancyOrigin_.x));
    minimumY = std::max(
        minimumY, static_cast<std::int64_t>(staticLightOccupancyOrigin_.y));
    maximumX = std::min(maximumX, occupancyRight - 1);
    maximumY = std::min(maximumY, occupancyBottom - 1);
    if (minimumX > maximumX || minimumY > maximumY) {
        return false;
    }

    const std::size_t firstX =
        static_cast<std::size_t>(minimumX - staticLightOccupancyOrigin_.x);
    const std::size_t firstY =
        static_cast<std::size_t>(minimumY - staticLightOccupancyOrigin_.y);
    const std::size_t lastX =
        static_cast<std::size_t>(maximumX - staticLightOccupancyOrigin_.x);
    const std::size_t lastY =
        static_cast<std::size_t>(maximumY - staticLightOccupancyOrigin_.y);
    const std::size_t prefixWidth = staticLightOccupancySize_.x + 1;
    const std::size_t topLeft = firstY * prefixWidth + firstX;
    const std::size_t topRight = firstY * prefixWidth + lastX + 1;
    const std::size_t bottomLeft = (lastY + 1) * prefixWidth + firstX;
    const std::size_t bottomRight = (lastY + 1) * prefixWidth + lastX + 1;
    const std::size_t included = staticLightOccupancyPrefix_[bottomRight] +
                                 staticLightOccupancyPrefix_[topLeft];
    const std::size_t excluded = staticLightOccupancyPrefix_[topRight] +
                                 staticLightOccupancyPrefix_[bottomLeft];
    const std::size_t total = included - excluded;
    return total > 0;
}

std::vector<LightOcclusionResult> GameMapBase::analyseLightOcclusion(
    const std::vector<LightOcclusionInput>& inputs,
    const std::vector<std::shared_ptr<Actor>>& visibleActors) {
    std::unordered_map<Actor*, std::shared_ptr<Actor>> visibleActorOwners;
    visibleActorOwners.reserve(visibleActors.size());
    for (const std::shared_ptr<Actor>& actor : visibleActors) {
        if (actor) {
            visibleActorOwners.try_emplace(actor.get(), actor);
        }
    }

    std::vector<LightOcclusionResult> results;
    results.reserve(inputs.size());
    for (const LightOcclusionInput& input : inputs) {
        LightOcclusionResult result;
        result.hasStaticTransmissionLoss = hasStaticLightOccupancy(input.light);
        const Light& light = input.light;
        if (light.radius <= 0.0f || !std::isfinite(light.position.x) ||
            !std::isfinite(light.position.y) || !std::isfinite(light.radius)) {
            results.push_back(std::move(result));
            continue;
        }

        const float cellSize = static_cast<float>(CellSize);
        const float rawCellRadius =
            std::ceil((light.radius + cellSize) / cellSize);
        const float rawMapX = std::floor(light.position.x / cellSize);
        const float rawMapY = std::floor(light.position.y / cellSize);
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
        const std::vector<Actor*> actors =
            getActorsInRangeImpl(mapX, mapY, cellRadius, input.owner.get());

        std::optional<sf::FloatRect> actorBounds;
        for (Actor* actor : actors) {
            const auto visibleActor = visibleActorOwners.find(actor);
            if (visibleActor == visibleActorOwners.end() ||
                actor->isDestroyed() || actor->getLightBlock() <= 0.0f) {
                continue;
            }
            const sf::FloatRect bounds = actor->getGlobalBounds();
            if (!actorIntersectsLight(bounds, light)) {
                continue;
            }
            result.occluders.push_back(visibleActor->second);
            if (!actorBounds.has_value()) {
                actorBounds = bounds;
                continue;
            }
            const float left =
                std::min(actorBounds->position.x, bounds.position.x);
            const float top =
                std::min(actorBounds->position.y, bounds.position.y);
            const float right =
                std::max(actorBounds->position.x + actorBounds->size.x,
                         bounds.position.x + bounds.size.x);
            const float bottom =
                std::max(actorBounds->position.y + actorBounds->size.y,
                         bounds.position.y + bounds.size.y);
            actorBounds =
                sf::FloatRect({left, top}, {right - left, bottom - top});
        }

        if (actorBounds.has_value()) {
            const float minimumX =
                std::max(std::floor(light.position.x - light.radius) -
                             kDynamicTransmissionPadding,
                         std::floor(actorBounds->position.x) -
                             kDynamicTransmissionPadding);
            const float minimumY =
                std::max(std::floor(light.position.y - light.radius) -
                             kDynamicTransmissionPadding,
                         std::floor(actorBounds->position.y) -
                             kDynamicTransmissionPadding);
            const float maximumX = std::min(
                std::ceil(light.position.x + light.radius) +
                    kDynamicTransmissionPadding,
                std::ceil(actorBounds->position.x + actorBounds->size.x) +
                    kDynamicTransmissionPadding);
            const float maximumY = std::min(
                std::ceil(light.position.y + light.radius) +
                    kDynamicTransmissionPadding,
                std::ceil(actorBounds->position.y + actorBounds->size.y) +
                    kDynamicTransmissionPadding);
            result.maskRect = sf::FloatRect(
                {minimumX, minimumY}, {std::max(1.0f, maximumX - minimumX),
                                       std::max(1.0f, maximumY - minimumY)});
        }
        results.push_back(std::move(result));
    }
    return results;
}
