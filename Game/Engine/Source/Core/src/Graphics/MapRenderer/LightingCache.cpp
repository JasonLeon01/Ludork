#include "MapRendererImpl.hpp"

#include <LudorkGenerated/LightingConstants.hpp>

#include <algorithm>
#include <stdexcept>

bool MapRenderer::Impl::StaticLightCache::matches(const Light& value,
                                                  sf::Vector2u size) const {
    return texture && texture->getSize() == size &&
           light.position == value.position && light.colour == value.colour &&
           light.radius == value.radius && light.intensity == value.intensity;
}

std::vector<std::size_t> MapRenderer::Impl::prepareLightCaches(
    const std::vector<Light>& lights, const std::vector<sf::Vector2u>& sizes) {
    constexpr std::size_t limit =
        ludork::generated::lighting::MaximumShaderLights;
    std::vector<std::size_t> indices(lights.size(), limit);
    std::vector<bool> reserved(limit, false);
    for (std::size_t index = 0; index < lights.size(); ++index) {
        const Light& light = lights[index];
        for (std::size_t slot = 0; slot < lightCaches_.size(); ++slot) {
            const StaticLightCache& cache = lightCaches_[slot];
            if (!reserved[slot] && cache.matches(light, sizes[index])) {
                if (indices[index] == limit ||
                    cache.generation == transmissionGeneration_) {
                    indices[index] = slot;
                }
                if (cache.generation == transmissionGeneration_) {
                    break;
                }
            }
        }
        if (indices[index] != limit) {
            reserved[indices[index]] = true;
        }
    }
    for (std::size_t index = 0; index < lights.size(); ++index) {
        if (indices[index] != limit) {
            continue;
        }
        for (std::size_t slot = 0; slot < lightCaches_.size(); ++slot) {
            const StaticLightCache& cache = lightCaches_[slot];
            if (!reserved[slot] && cache.texture &&
                cache.texture->getSize() == sizes[index]) {
                indices[index] = slot;
                reserved[slot] = true;
                break;
            }
        }
    }
    for (std::size_t index = 0; index < lights.size(); ++index) {
        if (indices[index] == limit) {
            std::size_t slot = limit;
            if (lightCaches_.size() < limit) {
                slot = lightCaches_.size();
                lightCaches_.emplace_back();
            } else {
                for (std::size_t candidate = 0; candidate < limit;
                     ++candidate) {
                    if (!reserved[candidate] &&
                        (slot == limit || lightCaches_[candidate].lastUse <
                                              lightCaches_[slot].lastUse)) {
                        slot = candidate;
                    }
                }
            }
            indices[index] = slot;
            reserved[slot] = true;
        }
        lightCaches_[indices[index]].lastUse = ++lightingCacheUse_;
    }
    return indices;
}

MapRenderer::Impl::DynamicLightTexture MapRenderer::Impl::dynamicLightTexture(
    sf::Vector2u size, bool transmission) {
    for (DynamicLightCache& cache : dynamicLightCaches_) {
        if (cache.size == size &&
            static_cast<bool>(cache.texture.transmission) == transmission) {
            cache.lastUse = ++lightingCacheUse_;
            return cache.texture;
        }
    }
    DynamicLightTexture texture;
    if (transmission) {
        texture.transmission = std::make_shared<sf::RenderTexture>(size);
        texture.transmission->setSmooth(false);
        ++statistics_.renderTextureAllocations;
    } else {
        texture.occupancy = createOccupancyTexture(size);
    }
    dynamicLightCaches_.push_back({size, ++lightingCacheUse_, texture});
    dynamicLightingCacheBytes_ +=
        static_cast<std::uint64_t>(size.x) * size.y * 4;
    trimDynamicLightingCache();
    return texture;
}

void MapRenderer::Impl::trimDynamicLightingCache() {
    constexpr std::uint64_t budget = 64u * 1024u * 1024u;
    constexpr std::size_t limit =
        ludork::generated::lighting::MaximumShaderLights;
    if (dynamicLightingCacheBytes_ <= budget &&
        dynamicLightCaches_.size() <= limit) {
        return;
    }
    std::size_t transmissions = 0;
    for (const DynamicLightCache& cache : dynamicLightCaches_) {
        transmissions += cache.texture.transmission ? 1 : 0;
    }
    std::size_t occupancies = dynamicLightCaches_.size() - transmissions;
    while (dynamicLightingCacheBytes_ > budget || transmissions > limit ||
           occupancies > limit) {
        auto oldest = dynamicLightCaches_.end();
        for (auto entry = dynamicLightCaches_.begin();
             entry != dynamicLightCaches_.end(); ++entry) {
            const bool transmission =
                static_cast<bool>(entry->texture.transmission);
            const long owners = transmission
                                    ? entry->texture.transmission.use_count()
                                    : entry->texture.occupancy.use_count();
            if (owners != 1 ||
                (dynamicLightingCacheBytes_ <= budget &&
                 (transmission ? transmissions : occupancies) <= limit)) {
                continue;
            }
            if (oldest == dynamicLightCaches_.end() ||
                entry->lastUse < oldest->lastUse) {
                oldest = entry;
            }
        }
        if (oldest == dynamicLightCaches_.end()) {
            return;
        }
        dynamicLightingCacheBytes_ -=
            static_cast<std::uint64_t>(oldest->size.x) * oldest->size.y * 4;
        if (oldest->texture.transmission) {
            --transmissions;
        } else {
            --occupancies;
        }
        dynamicLightCaches_.erase(oldest);
    }
}

void MapRenderer::Impl::prepareOccupancy(sf::Vector2u size) {
    const std::uint64_t pixels = static_cast<std::uint64_t>(size.x) * size.y;
    if (pixels > occupancyPixels_.max_size() / 4) {
        throw std::length_error("Light occupancy buffer is too large");
    }
    const std::size_t bytes = static_cast<std::size_t>(pixels) * 4;
    if (bytes > occupancyPixels_.size()) {
        const std::size_t capacity = occupancyPixels_.capacity();
        occupancyPixels_.resize(bytes);
        if (occupancyPixels_.capacity() > capacity) {
            ++statistics_.occupancyBufferGrowths;
        }
    }
    std::fill_n(occupancyPixels_.begin(), bytes, std::uint8_t{0});
    for (std::size_t pixel = 3; pixel < bytes; pixel += 4) {
        occupancyPixels_[pixel] = 255;
    }
    occupancyPixelSize_ = size;
}

void MapRenderer::Impl::fillOccupancy(const sf::IntRect& bounds) {
    const int left = std::max(0, bounds.position.x);
    const int top = std::max(0, bounds.position.y);
    const int right = std::min(static_cast<int>(occupancyPixelSize_.x),
                               bounds.position.x + bounds.size.x);
    const int bottom = std::min(static_cast<int>(occupancyPixelSize_.y),
                                bounds.position.y + bounds.size.y);
    if (left >= right || top >= bottom) {
        return;
    }
    for (int y = top; y < bottom; ++y) {
        const std::size_t offset =
            (static_cast<std::size_t>(occupancyPixelSize_.y - 1 - y) *
                 occupancyPixelSize_.x +
             left) *
            4;
        std::fill_n(occupancyPixels_.begin() + offset,
                    static_cast<std::size_t>(right - left) * 4,
                    std::uint8_t{255});
    }
}

std::shared_ptr<sf::Texture> MapRenderer::Impl::createOccupancyTexture(
    sf::Vector2u size) {
    auto texture = std::make_shared<sf::Texture>(size);
    ++statistics_.occupancyTextureAllocations;
    return texture;
}
