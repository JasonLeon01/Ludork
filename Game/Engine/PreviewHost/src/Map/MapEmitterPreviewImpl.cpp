#include "MapEmitterPreviewImpl.hpp"

#include "MapPreviewData.hpp"

#include <Emitters/EmitterResource.hpp>
#include <Runtime/Json.hpp>

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace ludork::preview_host {
namespace {

const RuntimeData& particleAsset(const RuntimeData::Map& particles,
                                 const std::string& key) {
    const RuntimeData* asset = map_preview_data::optional(particles, key);
    if (asset == nullptr || asset->isNil()) {
        throw std::invalid_argument("Map particle resource was not found: " +
                                    key);
    }
    const RuntimeData::Map& data = map_preview_data::map(*asset);
    const std::string error = map_preview_data::text(data, "previewError");
    if (!error.empty()) {
        throw std::invalid_argument(error);
    }
    if (const RuntimeData* tracks =
            map_preview_data::optional(data, "tracks")) {
        for (const RuntimeData& value : map_preview_data::array(*tracks)) {
            const RuntimeData::Map& track = map_preview_data::map(value);
            const RuntimeData* curves =
                map_preview_data::optional(track, "curves");
            if (curves == nullptr) {
                continue;
            }
            for (const auto& [channel, curve] :
                 map_preview_data::map(*curves)) {
                if (curve.getIf<std::string>() != nullptr) {
                    throw std::invalid_argument(
                        "Map particle curve must be an inline snapshot: " +
                        key + "." + channel);
                }
            }
        }
    }
    return *asset;
}

}  // namespace

void MapEmitterPreviewImpl::update(const RuntimeData::Map& data,
                                   const MapRenderer::Scene& scene) {
    const RuntimeData::Map& particles =
        map_preview_data::map(map_preview_data::required(data, "particles"));
    const RuntimeData::Array& sprites =
        map_preview_data::array(map_preview_data::required(data, "sprites"));
    std::unordered_set<std::uint64_t> retained;
    std::unordered_set<std::string> retainedResources;
    for (std::size_t index = 0; index < sprites.size(); ++index) {
        const RuntimeData::Map& fields = map_preview_data::map(sprites[index]);
        const RuntimeData* value =
            map_preview_data::optional(fields, "emitter");
        if (value == nullptr || value->isNil()) {
            continue;
        }
        const RuntimeData::Map& component = map_preview_data::map(*value);
        const std::string resource =
            map_preview_data::text(component, "resource");
        if (resource.empty()) {
            continue;
        }
        if (retainedResources.insert(resource).second) {
            const RuntimeData& asset = particleAsset(particles, resource);
            const std::string signature = stringifyJSON(asset);
            const auto previous = resources_.find(resource);
            if (previous == resources_.end() ||
                previous->second.signature != signature) {
                resources_.insert_or_assign(
                    resource,
                    Resource{
                        signature,
                        ludork::engine::emitters::parseEmitterConfiguration(
                            asset)});
            }
        }
        const Resource& configuration = resources_.at(resource);
        const MapRenderer::Sprite& sprite = scene.sprites.at(index);
        const auto previous = entries_.find(sprite.id);
        if (previous == entries_.end() ||
            previous->second.resource != resource ||
            previous->second.signature != configuration.signature) {
            std::unique_ptr<Emitter> emitter = std::make_unique<Emitter>();
            emitter->setConfiguration(configuration.configuration);
            if (previous != entries_.end()) {
                previous->second.emitter->shutdown();
            }
            Entry loaded;
            loaded.emitter = std::move(emitter);
            loaded.resource = resource;
            loaded.signature = configuration.signature;
            entries_.insert_or_assign(sprite.id, std::move(loaded));
        }
        Entry& entry = entries_.at(sprite.id);
        const sf::Vector2f bounds =
            map_preview_data::vector(component, "bounds");
        if (bounds.x < 0 || bounds.y < 0) {
            throw std::invalid_argument(
                "Map emitter bounds must be nonnegative");
        }
        const sf::Vector2f anchor =
            map_preview_data::vector(component, "anchor", {0.5f, 0.5f});
        const sf::Vector2f offset =
            map_preview_data::vector(component, "offset");
        entry.transform = sprite.transform;
        entry.transform.translate(
            {bounds.x * anchor.x + offset.x, bounds.y * anchor.y + offset.y});
        entry.transform.rotate(
            sf::degrees(map_preview_data::number(component, "rotation", 0)));
        entry.transform.scale(
            map_preview_data::vector(component, "scale", {1, 1}));
        entry.beforeActor =
            map_preview_data::boolean(component, "beforeActor", false);
        entry.colour = {
            255, 255, 255,
            static_cast<std::uint8_t>(
                std::clamp(sprite.presentationOpacity, 0.0f, 1.0f) * 255.0f)};
        retained.insert(sprite.id);
    }
    std::erase_if(entries_, [&retained](const auto& item) {
        if (retained.contains(item.first)) {
            return false;
        }
        if (item.second.emitter != nullptr) {
            item.second.emitter->shutdown();
        }
        return true;
    });
    std::erase_if(resources_, [&retainedResources](const auto& item) {
        return !retainedResources.contains(item.first);
    });
    Emitter::collectGarbage();
}

void MapEmitterPreviewImpl::advance(float delta, bool resetMotion) {
    Emitter::collectGarbage();
    for (auto& [id, entry] : entries_) {
        entry.emitter->setHostTransform(entry.transform);
        entry.emitter->setColour(entry.colour);
        if (resetMotion || delta == 0) {
            entry.emitter->resetHostMotion();
        }
        if (entry.initialize) {
            entry.emitter->tick(0);
            entry.initialize = false;
        } else if (delta > 0) {
            entry.emitter->tick(delta);
        }
    }
}

MapRenderer::DrawHooks MapEmitterPreviewImpl::drawHooks() {
    MapRenderer::DrawHooks hooks;
    hooks.beforeSprite = [this](std::uint64_t id, sf::RenderTarget& target,
                                const sf::RenderStates& states) {
        draw(id, target, states, true);
    };
    hooks.afterSprite = [this](std::uint64_t id, sf::RenderTarget& target,
                               const sf::RenderStates& states) {
        draw(id, target, states, false);
    };
    return hooks;
}

void MapEmitterPreviewImpl::draw(std::uint64_t id, sf::RenderTarget& target,
                                 const sf::RenderStates& states, bool before) {
    const auto found = entries_.find(id);
    if (found != entries_.end() && found->second.beforeActor == before) {
        found->second.emitter->draw(target, states);
    }
}

void MapEmitterPreviewImpl::clear() noexcept {
    for (auto& [id, entry] : entries_) {
        if (entry.emitter != nullptr) {
            entry.emitter->shutdown();
        }
    }
    entries_.clear();
    resources_.clear();
    Emitter::collectGarbage();
}

}  // namespace ludork::preview_host
