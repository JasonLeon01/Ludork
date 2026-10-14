#include "GameMapRendererImpl.hpp"

#include <Fog/FogController.hpp>
#include <Panorama/PanoramaController.hpp>
#include <Graphics.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>

MapRenderer::Sprite GameMapRendererImpl::actorSprite(
    const std::shared_ptr<Actor>& actor, const std::string& layer,
    int alpha) const {
    MapRenderer::Sprite sprite;
    sprite.id = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(actor.get()));
    sprite.layer = layer;
    sprite.texture = actor->getSpriteTexture();
    sprite.textureRect = actor->getTextureRect();
    sprite.transform = actor->getTransform();
    sprite.material = actor->getMaterial();
    sprite.colour = {255, 255, 255,
                     static_cast<std::uint8_t>(std::clamp(alpha, 0, 255))};
    sprite.materialParameters = actor->getMaterialParameters();
    sprite.materialResource = actor->getMaterialResource();
    return sprite;
}

void GameMapRendererImpl::buildScene(bool applyCover, int materialRevision) {
    prepareVisibleLayers();
    scene = {};
    scene.cellSize = map.getSize();
    scene.layerOrder = layerNames;
    scene.regions = map.getRenderRegions();
    const bool world = !scene.regions.empty();
    if (!world) {
        scene.regions.push_back({"", {}, tilemap});
    }
    actorOwners.clear();
    if (previewSpritesInstalled) {
        refreshPreviewVisibility();
        for (std::size_t index = 0; index < previewSprites.size(); ++index) {
            const PreviewState& entry = previewSprites[index];
            MapRenderer::Sprite sprite;
            sprite.id = index;
            sprite.layer = entry.data.layer;
            sprite.texture = entry.data.texture;
            sprite.textureRect = entry.data.rect;
            sprite.transform = entry.transform;
            sprite.material = entry.data.material;
            sprite.materialParameters = entry.data.materialParameters;
            sprite.materialResource = entry.materialResource;
            sprite.visible = entry.visibleOnMap;
            scene.sprites.push_back(std::move(sprite));
        }
        return;
    }
    const int playerLayer = applyCover && !world ? playerLayerIndex() : -1;
    sf::Vector2i playerPosition;
    if (applyCover && !world &&
        preparePlayerCover(playerLayer, materialRevision, playerPosition)) {
        for (std::size_t index = 0; index < layerNames.size(); ++index) {
            const std::shared_ptr<TileLayer> layer =
                tilemap->getLayer(layerNames[index]);
            if (layer && layer->getVisible()) {
                applyPlayerCover(*layer, static_cast<int>(index), playerLayer,
                                 playerPosition);
            }
        }
    }
    const std::shared_ptr<Actor>& player = map.getPlayerActorForRenderer();
    const ActorDict& actors = map.getMaterialActorsForRenderer();
    for (std::size_t index = 0; index < layerNames.size(); ++index) {
        const std::string& name = layerNames[index];
        const auto found = actors.find(name);
        if (found == actors.end()) {
            continue;
        }
        for (const std::shared_ptr<Actor>& actor : found->second) {
            if (!actor || !map.isActorVisibleOnMap(*actor) ||
                (actorVisibilityFilter &&
                 !actorVisibilityFilter(*actor, name))) {
                continue;
            }
            int alpha = 255;
            if (playerLayer >= 0 && static_cast<int>(index) > playerLayer &&
                player && actor != player && actor->intersects(*player)) {
                alpha = coverAlpha;
            }
            MapRenderer::Sprite sprite = actorSprite(actor, name, alpha);
            if (effectHiddenActors.contains(actor.get())) {
                sprite.presentationOpacity = 0.0f;
            }
            actorOwners.emplace(sprite.id, actor);
            if (actor->lightComp && actor->lightComp->lightRadius > 0.0f) {
                const sf::FloatRect bounds = actor->getLocalBounds();
                const sf::Vector2f local = bounds.position +
                                           bounds.size * 0.5f +
                                           actor->lightComp->lightOffset;
                scene.lights.push_back(
                    {actor->getTransform().transformPoint(local),
                     actor->lightComp->lightColour,
                     actor->lightComp->lightRadius, 1.0f, sprite.id});
            }
            scene.sprites.push_back(std::move(sprite));
        }
    }
}

void GameMapRendererImpl::drawContent(
    sf::RenderTarget& target, const sf::RenderStates& states, bool applyCover,
    float time, int materialRevision,
    const std::function<void(const std::string&, sf::RenderTarget&,
                             const sf::RenderStates&)>& effects) {
    buildScene(applyCover, materialRevision);
    MapRenderer::Options options;
    options.time = time;
    renderer.drawContent(scene, target, states, options, drawHooks(effects));
}

MapRenderer::DrawHooks GameMapRendererImpl::drawHooks(
    const std::function<void(const std::string&, sf::RenderTarget&,
                             const sf::RenderStates&)>& effects) {
    MapRenderer::DrawHooks hooks;
    hooks.beforeSprite = [this](std::uint64_t id, sf::RenderTarget& output,
                                const sf::RenderStates& drawStates) {
        const auto found = actorOwners.find(id);
        if (found != actorOwners.end() &&
            !effectHiddenActors.contains(found->second.get())) {
            found->second->drawEmitter(output, drawStates, true);
        }
    };
    hooks.afterSprite = [this](std::uint64_t id, sf::RenderTarget& output,
                               const sf::RenderStates& drawStates) {
        const auto found = actorOwners.find(id);
        if (found != actorOwners.end() &&
            !effectHiddenActors.contains(found->second.get())) {
            found->second->drawEmitter(output, drawStates, false);
            found->second->drawBillboard(output, drawStates);
        }
    };
    hooks.afterLayer = effects;
    return hooks;
}

void GameMapRendererImpl::drawActor(sf::RenderTarget& target,
                                    const sf::RenderStates& states,
                                    const std::shared_ptr<Actor>& actor,
                                    int alpha, float time) {
    if (actor && map.isActorVisibleOnMap(*actor)) {
        renderer.drawSprite(actorSprite(actor, "", alpha), target, states,
                            time);
    }
}

MapRenderer::View GameMapRendererImpl::renderView() const {
    if (!camera || !camera->getViewPosition() || !camera->getViewSize() ||
        !camera->getRenderTexture()) {
        throw std::logic_error("Map composition requires a live camera");
    }
    return {{*camera->getViewPosition(), *camera->getViewSize()},
            camera->getViewRotation(),
            camera->getRenderTexture()->getSize()};
}

void GameMapRendererImpl::renderFrame(
    const sf::Color& ambient, float time, bool applyPlayerCover,
    int materialRevision,
    const std::function<void(const std::string&, sf::RenderTarget&,
                             const sf::RenderStates&)>& effects,
    const std::function<void()>& overlays) {
    buildScene(applyPlayerCover, materialRevision);
    scene.ambientLight = ambient;
    scene.panorama = PanoramaController::getTexture();
    if (fogController) {
        fogController->applyToScene(scene);
    }
    MapRenderer::Options options;
    options.lighting = MapRenderer::LightingMode::Lit;
    options.time = time;
    options.lightingScale = Graphics::getLightingRenderScale();
    const MapRenderer::View view = renderView();
    const auto canvas = camera->getRenderTexture();
    const sf::Texture* overlayTexture = nullptr;
    if (overlays) {
        if (!overlay || overlay->getSize() != view.pixelSize) {
            overlay = std::make_shared<sf::RenderTexture>(view.pixelSize);
        }
        overlay->setView(canvas->getView());
        overlay->clear(sf::Color::Transparent);
        camera->withRenderTarget(overlay, overlays);
        overlay->display();
        overlayTexture = &overlay->getTexture();
    }
    const sf::RenderTexture& result = renderer.render(
        scene, view, options, drawHooks(effects), overlayTexture);
    const sf::View saved = canvas->getView();
    canvas->setView(canvas->getDefaultView());
    canvas->clear(sf::Color::Transparent);
    canvas->draw(sf::Sprite(result.getTexture()),
                 sf::RenderStates(sf::BlendNone));
    canvas->display();
    canvas->setView(saved);
}

void GameMapRendererImpl::prewarm(const sf::Color& ambient, float time) {
    buildScene(false, 0);
    scene.ambientLight = ambient;
    scene.panorama = PanoramaController::getTexture();
    if (fogController) {
        fogController->applyToScene(scene);
    }
    MapRenderer::Options options;
    options.lighting = MapRenderer::LightingMode::Lit;
    options.time = time;
    options.lightingScale = Graphics::getLightingRenderScale();
    renderer.render(scene, renderView(), options);
}
