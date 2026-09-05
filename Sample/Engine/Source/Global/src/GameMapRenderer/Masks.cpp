#include "Impl.hpp"

#include <EngineState.hpp>

#include <algorithm>
#include <utility>

bool GameMapRendererImpl::ActorState::matches(const Actor& value,
                                              bool fullMaterial) const {
    const std::shared_ptr<sf::Texture> valueTexture = value.getSpriteTexture();
    if (actor != &value || texture != valueTexture.get() ||
        position != value.getPosition() ||
        translation != value.getTranslation() || scale != value.getScale() ||
        origin != value.getOrigin() || rotation != value.getRotation() ||
        textureRect != value.getTextureRect() ||
        colour.a != value.getColor().a || lightBlock != value.getLightBlock()) {
        return false;
    }
    if (!fullMaterial) {
        return true;
    }
    const sf::Color valueColour = value.getColor();
    return colour.r == valueColour.r && colour.g == valueColour.g &&
           colour.b == valueColour.b && mirror == value.getMirror() &&
           reflectionStrength == value.getReflectionStrength() &&
           ignoreLighting == value.getIgnoreLighting();
}

GameMapRendererImpl::ActorState GameMapRendererImpl::captureActor(
    const std::shared_ptr<Actor>& actor) {
    ActorState state;
    if (!actor) {
        return state;
    }
    const std::shared_ptr<sf::Texture> texture = actor->getSpriteTexture();
    state.actor = actor.get();
    state.texture = texture.get();
    state.position = actor->getPosition();
    state.translation = actor->getTranslation();
    state.scale = actor->getScale();
    state.origin = actor->getOrigin();
    state.rotation = actor->getRotation();
    state.textureRect = actor->getTextureRect();
    state.colour = actor->getColor();
    state.lightBlock = actor->getLightBlock();
    state.mirror = actor->getMirror();
    state.reflectionStrength = actor->getReflectionStrength();
    state.ignoreLighting = actor->getIgnoreLighting();
    return state;
}

std::vector<GameMapRendererImpl::ActorState> GameMapRendererImpl::captureActors(
    const std::vector<std::shared_ptr<Actor>>& actors) const {
    std::vector<ActorState> result;
    result.reserve(actors.size());
    for (const std::shared_ptr<Actor>& actor : actors) {
        result.push_back(captureActor(actor));
    }
    return result;
}

std::vector<bool> GameMapRendererImpl::captureLayerVisibility() const {
    std::vector<bool> result;
    result.reserve(layerNames.size());
    for (const std::string& layerName : layerNames) {
        const std::shared_ptr<TileLayer> layer = tilemap->getLayer(layerName);
        result.push_back(layer && layer->getVisible());
    }
    return result;
}

bool GameMapRendererImpl::staticTransmissionMatches(
    const std::vector<std::shared_ptr<Actor>>& actors,
    int materialRevision) const {
    if (staticTransmissionRevision != materialRevision ||
        staticTransmissionLayers != captureLayerVisibility() ||
        staticTransmissionCoverPosition != coverPlayerPosition ||
        staticTransmissionCoverLayer != coverPlayerLayerIndex ||
        staticTransmissionActors.size() != actors.size()) {
        return false;
    }
    for (std::size_t index = 0; index < actors.size(); ++index) {
        if (!actors[index] ||
            !staticTransmissionActors[index].matches(*actors[index], false)) {
            return false;
        }
    }
    return true;
}

bool GameMapRendererImpl::surfaceMaskMatches(
    const std::vector<std::shared_ptr<Actor>>& actors,
    int materialRevision) const {
    if (surfaceMaskRevision != materialRevision ||
        surfaceMaskLayers != captureLayerVisibility() ||
        surfaceMaskCoverPosition != coverPlayerPosition ||
        surfaceMaskCoverLayer != coverPlayerLayerIndex ||
        surfaceMaskActors.size() != actors.size()) {
        return false;
    }
    for (std::size_t index = 0; index < actors.size(); ++index) {
        if (!actors[index] ||
            !surfaceMaskActors[index].matches(*actors[index], true)) {
            return false;
        }
    }
    return true;
}

void GameMapRendererImpl::rebuildStaticTransmission(
    const std::vector<std::shared_ptr<Actor>>& staticActors,
    int materialRevision) {
    if (staticTransmissionMatches(staticActors, materialRevision)) {
        return;
    }
    staticTransmission->clear(sf::Color::White);
    tileMaskShader->setUniform("transmissionMode", 1.0f);
    for (const std::string& layerName : layerNames) {
        const std::shared_ptr<TileLayer> layer = tilemap->getLayer(layerName);
        if (!layer || !layer->getVisible()) {
            continue;
        }
        setTileMaskUniforms(layerName, *layer);
        staticTransmission->draw(*layer, transmissionTileStates);
    }
    actorMaskShader->setUniform("transmissionMode", 1.0f);
    for (const std::shared_ptr<Actor>& actor : staticActors) {
        if (!actor) {
            continue;
        }
        setActorMaskUniforms(*actor);
        staticTransmission->draw(*actor, transmissionActorStates);
    }
    staticTransmission->display();
    staticOccupancy = map.rebuildStaticLightOccupancy(
        {0, 0}, tilemap->getSize(), staticActors);
    staticTransmissionActors = captureActors(staticActors);
    staticTransmissionLayers = captureLayerVisibility();
    staticTransmissionCoverPosition = coverPlayerPosition;
    staticTransmissionCoverLayer = coverPlayerLayerIndex;
    staticTransmissionRevision = materialRevision;
    ++staticTransmissionGeneration;
    cachedStaticMaterialRevision = -1;
}

std::vector<std::shared_ptr<Actor>> GameMapRendererImpl::renderSurfaceMask(
    int materialRevision) {
    std::vector<std::shared_ptr<Actor>> visibleActors;
    std::unordered_set<Actor*> visibleActorSet;
    const ActorDict& actors = map.getMaterialActorsForRenderer();
    for (const std::string& layerName : layerNames) {
        const std::shared_ptr<TileLayer> layer = tilemap->getLayer(layerName);
        if (!layer || !layer->getVisible()) {
            continue;
        }
        const auto layerIt = actors.find(layerName);
        if (layerIt == actors.end()) {
            continue;
        }
        for (const std::shared_ptr<Actor>& actor : layerIt->second) {
            if (actor && !actor->isDestroyed() &&
                visibleActorSet.insert(actor.get()).second) {
                visibleActors.push_back(actor);
            }
        }
    }
    if (surfaceMaskMatches(visibleActors, materialRevision)) {
        return visibleActors;
    }

    surfaceMask->clear(sf::Color::Transparent);
    tileMaskShader->setUniform("transmissionMode", 0.0f);
    actorMaskShader->setUniform("transmissionMode", 0.0f);
    for (const std::string& layerName : layerNames) {
        const std::shared_ptr<TileLayer> layer = tilemap->getLayer(layerName);
        if (!layer || !layer->getVisible()) {
            continue;
        }
        setTileMaskUniforms(layerName, *layer);
        surfaceMask->draw(*layer, surfaceTileStates);
        const auto layerIt = actors.find(layerName);
        if (layerIt == actors.end()) {
            continue;
        }
        for (const std::shared_ptr<Actor>& actor : layerIt->second) {
            if (!actor || actor->isDestroyed()) {
                continue;
            }
            setActorMaskUniforms(*actor);
            surfaceMask->draw(*actor, surfaceActorStates);
        }
    }
    surfaceMask->display();
    surfaceMaskActors = captureActors(visibleActors);
    surfaceMaskLayers = captureLayerVisibility();
    surfaceMaskCoverPosition = coverPlayerPosition;
    surfaceMaskCoverLayer = coverPlayerLayerIndex;
    surfaceMaskRevision = materialRevision;
    return visibleActors;
}

void GameMapRendererImpl::setTileMaskUniforms(const std::string& cacheKey,
                                              TileLayer& layer) {
    const std::shared_ptr<sf::Image> lightBlockImage =
        layer.getLightBlockImage();
    const std::shared_ptr<sf::Image> reflectionImage =
        layer.getReflectionStrengthImage();
    const std::shared_ptr<sf::Image> ignoreLightingImage =
        layer.getIgnoreLightingImage();
    LayerMaskTextures& cache = layerMaskTextures[cacheKey];
    if (cache.lightBlockImage != lightBlockImage ||
        cache.reflectionImage != reflectionImage ||
        cache.ignoreLightingImage != ignoreLightingImage) {
        cache.lightBlockImage = lightBlockImage;
        cache.reflectionImage = reflectionImage;
        cache.ignoreLightingImage = ignoreLightingImage;
        cache.lightBlockTexture =
            std::make_shared<sf::Texture>(*lightBlockImage);
        cache.reflectionTexture =
            std::make_shared<sf::Texture>(*reflectionImage);
        cache.ignoreLightingTexture =
            std::make_shared<sf::Texture>(*ignoreLightingImage);
    }
    tileMaskShader->setUniform("lightBlockTex", *cache.lightBlockTexture);
    tileMaskShader->setUniform("reflectionStrengthTex",
                               *cache.reflectionTexture);
    tileMaskShader->setUniform("ignoreLightingTex",
                               *cache.ignoreLightingTexture);
    tileMaskShader->setUniform("lightBlockSize",
                               sf::Vector2f{static_cast<float>(CellSize),
                                            static_cast<float>(CellSize)});
    tileMaskShader->setUniform("worldMode", 0.0f);
    const sf::Vector2u size = tilemap->getSize();
    tileMaskShader->setUniform(
        "mapSize",
        sf::Vector2f{static_cast<float>(size.x), static_cast<float>(size.y)});
}

void GameMapRendererImpl::setActorMaskUniforms(const Actor& actor) {
    actorMaskShader->setUniform("lightBlock", actor.getLightBlock());
    actorMaskShader->setUniform(
        "reflectionStrength",
        actor.getMirror() ? actor.getReflectionStrength() : 0.0f);
    actorMaskShader->setUniform("ignoreLighting",
                                actor.getIgnoreLighting() ? 1.0f : 0.0f);
}
