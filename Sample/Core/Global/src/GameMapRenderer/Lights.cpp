#include "Impl.hpp"

#include <Runtime/EngineState.hpp>
#include <System.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr std::size_t MaximumShaderLights = 16;

}  // namespace

bool GameMapRendererImpl::LightState::matches(const ActiveLight& value) const {
    return owner == value.owner.get() &&
           light.position == value.light.position &&
           light.colour == value.light.colour &&
           light.radius == value.light.radius &&
           light.intensity == value.light.intensity;
}

bool GameMapRendererImpl::lightVisible(
    const Light& light, const std::optional<sf::FloatRect>& viewport,
    sf::Angle rotation) {
    if (!viewport) {
        return true;
    }
    const float radians = rotation.asRadians();
    const float cosine = std::abs(std::cos(radians));
    const float sine = std::abs(std::sin(radians));
    const float halfWidth = viewport->size.x * 0.5f;
    const float halfHeight = viewport->size.y * 0.5f;
    const float extentX = cosine * halfWidth + sine * halfHeight;
    const float extentY = sine * halfWidth + cosine * halfHeight;
    const float centreX = viewport->position.x + halfWidth;
    const float centreY = viewport->position.y + halfHeight;
    return light.position.x + light.radius >= centreX - extentX &&
           light.position.x - light.radius <= centreX + extentX &&
           light.position.y + light.radius >= centreY - extentY &&
           light.position.y - light.radius <= centreY + extentY;
}

std::vector<GameMapRendererImpl::ActiveLight>
GameMapRendererImpl::collectActiveLights(
    const std::vector<Light>& mapLights) const {
    std::vector<ActiveLight> result;
    result.reserve(MaximumShaderLights);
    const std::optional<sf::FloatRect> viewport =
        camera ? camera->getViewport() : std::nullopt;
    const sf::Angle rotation =
        camera ? camera->getViewRotation() : sf::degrees(0.0f);
    for (const Light& light : mapLights) {
        if (light.radius > 0.0f && lightVisible(light, viewport, rotation)) {
            result.push_back({light, nullptr});
            if (result.size() == MaximumShaderLights) {
                return result;
            }
        }
    }

    const ActorDict& actors = map.getMaterialActorsForRenderer();
    for (const auto& [_, actorList] : actors) {
        for (const std::shared_ptr<Actor>& actor : actorList) {
            if (!actor || actor->isDestroyed() || !actor->lightComp ||
                actor->lightComp->lightRadius <= 0.0f) {
                continue;
            }
            const sf::FloatRect bounds = actor->getLocalBounds();
            const sf::Vector2f offset = actor->lightComp->lightOffset;
            const sf::Vector2f localPosition{
                bounds.position.x + bounds.size.x * 0.5f + offset.x,
                bounds.position.y + bounds.size.y * 0.5f + offset.y,
            };
            Light light(actor->getTransform().transformPoint(localPosition),
                        actor->lightComp->lightColour,
                        actor->lightComp->lightRadius, 1.0f);
            if (!lightVisible(light, viewport, rotation)) {
                continue;
            }
            result.push_back({light, actor});
            if (result.size() == MaximumShaderLights) {
                return result;
            }
        }
    }
    return result;
}

std::pair<std::vector<std::shared_ptr<Actor>>,
          std::vector<std::shared_ptr<Actor>>>
GameMapRendererImpl::partitionLightBlockingActors(
    const std::vector<std::shared_ptr<Actor>>& visibleActors) const {
    std::vector<std::shared_ptr<Actor>> dynamicActors;
    std::vector<std::shared_ptr<Actor>> staticActors;
    for (const std::shared_ptr<Actor>& actor : visibleActors) {
        if (!actor || actor->getLightBlock() <= 0.0f) {
            continue;
        }
        if (actor->lightComp || actor->getAnimatable() || actor->isMoving()) {
            dynamicActors.push_back(actor);
        } else {
            staticActors.push_back(actor);
        }
    }
    return {std::move(dynamicActors), std::move(staticActors)};
}

void GameMapRendererImpl::renderLighting(const std::vector<Light>& mapLights,
                                         const sf::Color&,
                                         int materialRevision) {
    if (!camera || !materialShader || !tileMaskShader || !actorMaskShader ||
        !lightPassShader || !unobstructedLightShader || !staticTransmission ||
        !surfaceMask) {
        return;
    }
    const std::vector<std::shared_ptr<Actor>> visibleActors =
        renderSurfaceMask(materialRevision);
    auto [dynamicActors, staticActors] =
        partitionLightBlockingActors(visibleActors);
    const std::vector<ActiveLight> activeLights =
        collectActiveLights(mapLights);
    if (!activeLights.empty()) {
        rebuildStaticTransmission(staticActors, materialRevision);
    }
    const bool directLightChanged = ensureDirectLight();
    if (!directLightChanged &&
        renderedLightingMatches(activeLights, dynamicActors)) {
        return;
    }

    std::vector<LightOcclusionInput> inputs;
    inputs.reserve(activeLights.size());
    for (const ActiveLight& activeLight : activeLights) {
        inputs.push_back({activeLight.light, activeLight.owner});
    }
    const std::vector<LightOcclusionResult> analyses =
        activeLights.empty() ? std::vector<LightOcclusionResult>{}
                             : map.analyseLightOcclusion(inputs, dynamicActors);
    useStaticDirectLight = !activeLights.empty();
    for (const LightOcclusionResult& analysis : analyses) {
        if (!analysis.occluders.empty()) {
            useStaticDirectLight = false;
            break;
        }
    }
    if (useStaticDirectLight) {
        renderCachedLighting(activeLights, analyses, materialRevision);
        cacheRenderedLighting(activeLights, dynamicActors);
        return;
    }
    if (activeLights.empty()) {
        if (directLightChanged || !directLightCleared) {
            directLight->setView(camera->getView());
            directLight->clear(sf::Color::Black);
            directLight->display();
            directLightCleared = true;
        }
        cacheRenderedLighting(activeLights, dynamicActors);
        return;
    }
    directLightCleared = false;
    directLight->setView(camera->getView());
    directLight->clear(sf::Color::Black);
    renderDynamicLighting(activeLights, analyses);
    directLight->display();
    cacheRenderedLighting(activeLights, dynamicActors);
}

bool GameMapRendererImpl::ensureDirectLight() {
    const std::optional<sf::Vector2f> viewSize = camera->getViewSize();
    if (!viewSize) {
        return false;
    }
    const sf::Vector2u logicalSize{
        static_cast<unsigned int>(std::max(1.0f, std::floor(viewSize->x))),
        static_cast<unsigned int>(std::max(1.0f, std::floor(viewSize->y))),
    };
    const float scale = System::getLightingRenderScale();
    const sf::Vector2u requiredSize = lightingTargetSize(logicalSize, scale);
    if (directLight && directLight->getSize() == requiredSize) {
        directLight->setSmooth(scale < 1.0f);
        return false;
    }
    directLight = std::make_shared<sf::RenderTexture>(requiredSize);
    directLight->setSmooth(scale < 1.0f);
    renderedLightingValid = false;
    return true;
}

void GameMapRendererImpl::ensureStaticDirectLight() {
    const sf::Vector2u mapSize = tilemap->getSize();
    const unsigned int cellSize =
        static_cast<unsigned int>(std::max(CellSize, 1));
    const sf::Vector2u logicalSize{
        std::max(1u, mapSize.x * cellSize),
        std::max(1u, mapSize.y * cellSize),
    };
    const float scale = System::getLightingRenderScale();
    const sf::Vector2u requiredSize = lightingTargetSize(logicalSize, scale);
    if (staticDirectLight && staticDirectLight->getSize() == requiredSize) {
        staticDirectLight->setSmooth(scale < 1.0f);
        return;
    }
    staticDirectLight = std::make_shared<sf::RenderTexture>(requiredSize);
    staticDirectLight->setSmooth(scale < 1.0f);
    cachedStaticMaterialRevision = -1;
}

bool GameMapRendererImpl::lightsMatch(
    const std::vector<ActiveLight>& lights,
    const std::vector<LightState>& cache) const {
    if (lights.size() != cache.size()) {
        return false;
    }
    for (std::size_t index = 0; index < lights.size(); ++index) {
        if (!cache[index].matches(lights[index])) {
            return false;
        }
    }
    return true;
}

std::vector<GameMapRendererImpl::LightState> GameMapRendererImpl::captureLights(
    const std::vector<ActiveLight>& lights) const {
    std::vector<LightState> result;
    result.reserve(lights.size());
    for (const ActiveLight& light : lights) {
        result.push_back({light.light, light.owner.get()});
    }
    return result;
}

bool GameMapRendererImpl::renderedLightingMatches(
    const std::vector<ActiveLight>& lights,
    const std::vector<std::shared_ptr<Actor>>& dynamicActors) const {
    if (!renderedLightingValid || !directLight ||
        renderedStaticGeneration != staticTransmissionGeneration ||
        !lightsMatch(lights, renderedLights) ||
        renderedDynamicActors.size() != dynamicActors.size()) {
        return false;
    }
    for (std::size_t index = 0; index < dynamicActors.size(); ++index) {
        if (!dynamicActors[index] || !renderedDynamicActors[index].matches(
                                         *dynamicActors[index], true)) {
            return false;
        }
    }
    return renderedViewPosition == camera->getViewPosition() &&
           renderedViewSize == camera->getViewSize() &&
           renderedViewRotation == camera->getViewRotation() &&
           renderedTargetSize == directLight->getSize();
}

void GameMapRendererImpl::cacheRenderedLighting(
    const std::vector<ActiveLight>& lights,
    const std::vector<std::shared_ptr<Actor>>& dynamicActors) {
    renderedLights = captureLights(lights);
    renderedDynamicActors = captureActors(dynamicActors);
    renderedViewPosition = camera->getViewPosition();
    renderedViewSize = camera->getViewSize();
    renderedViewRotation = camera->getViewRotation();
    renderedTargetSize = directLight->getSize();
    renderedStaticGeneration = staticTransmissionGeneration;
    renderedLightingValid = true;
}

void GameMapRendererImpl::refreshMaterialShader(const sf::Color& ambientLight) {
    if (!camera || !materialShader || !surfaceMask || !directLight) {
        return;
    }
    const std::optional<sf::Vector2f> screenSize = camera->getViewSize();
    if (!screenSize) {
        return;
    }
    materialShader->setUniform("surfaceMask", surfaceMask->getTexture());
    materialShader->setUniform("directLight", directLight->getTexture());
    if (staticDirectLight) {
        materialShader->setUniform("staticDirectLight",
                                   staticDirectLight->getTexture());
    }
    materialShader->setUniform("useStaticDirectLight",
                               useStaticDirectLight ? 1.0f : 0.0f);
    setViewShaderUniforms(*materialShader, *screenSize, {0.0f, 0.0f}, false);
    materialShader->setUniform("ambientColor",
                               shaderColour(ambientLight, true));
}
