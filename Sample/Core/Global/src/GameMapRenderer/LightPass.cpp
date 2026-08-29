#include "Impl.hpp"

#include <Runtime/EngineState.hpp>
#include <System.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace {

constexpr unsigned int DynamicTransmissionPadding = 2;

}  // namespace

void GameMapRendererImpl::ensureDynamicTransmission(
    const std::vector<ActiveLight>& lights) {
    unsigned int requiredSize = 1;
    for (const ActiveLight& entry : lights) {
        const Light& light = entry.light;
        const float left = std::floor(light.position.x - light.radius);
        const float right = std::ceil(light.position.x + light.radius);
        const float top = std::floor(light.position.y - light.radius);
        const float bottom = std::ceil(light.position.y + light.radius);
        requiredSize = std::max(
            requiredSize,
            static_cast<unsigned int>(std::max(right - left, bottom - top) +
                                      DynamicTransmissionPadding * 2));
    }
    if (dynamicTransmission && dynamicTransmissionSize >= requiredSize) {
        return;
    }
    dynamicTransmission = std::make_shared<sf::RenderTexture>(
        sf::Vector2u{requiredSize, requiredSize});
    dynamicTransmission->setSmooth(false);
    dynamicTransmissionSize = requiredSize;
}

void GameMapRendererImpl::renderDynamicLighting(
    const std::vector<ActiveLight>& lights,
    const std::vector<LightOcclusionResult>& analyses) {
    std::vector<ActiveLight> unobstructedLights;
    std::vector<const sf::Texture*> staticTextures(lights.size(), nullptr);
    ensureDynamicTransmission(lights);
    for (std::size_t index = 0; index < lights.size(); ++index) {
        if (analyses[index].hasStaticTransmissionLoss) {
            staticTextures[index] =
                &ensureStaticLightCache(index, lights[index]);
        }
    }
    bool commonUniformsSet = false;
    for (std::size_t index = 0; index < lights.size(); ++index) {
        const auto [dynamicOrigin, dynamicSize, hasDynamicTransmission] =
            renderDynamicTransmission(analyses[index]);
        const sf::Texture* staticTexture = staticTextures[index];
        if (hasDynamicTransmission || staticTexture != nullptr) {
            if (!commonUniformsSet) {
                setLightPassCommonUniforms();
                commonUniformsSet = true;
            }
            if (staticTexture != nullptr) {
                lightPassShader->setUniform("cachedStaticLight",
                                            *staticTexture);
                lightPassShader->setUniform("useCachedStaticLight", 1.0f);
            } else {
                lightPassShader->setUniform("useCachedStaticLight", 0.0f);
            }
            renderLight(lights[index], dynamicOrigin, dynamicSize, false,
                        hasDynamicTransmission, *directLight);
        } else {
            unobstructedLights.push_back(lights[index]);
        }
    }
    lightPassShader->setUniform("useCachedStaticLight", 0.0f);
    renderUnobstructedLights(unobstructedLights, *directLight);
}

void GameMapRendererImpl::renderCachedLighting(
    const std::vector<ActiveLight>& lights,
    const std::vector<LightOcclusionResult>& analyses, int materialRevision) {
    ensureStaticDirectLight();
    const bool cacheValid =
        cachedStaticMaterialRevision == materialRevision &&
        cachedStaticTransmissionGeneration == staticTransmissionGeneration &&
        lightsMatch(lights, cachedStaticLights);
    if (cacheValid) {
        return;
    }
    std::vector<ActiveLight> unobstructedLights;
    std::vector<ActiveLight> staticLights;
    for (std::size_t index = 0; index < lights.size(); ++index) {
        if (analyses[index].hasStaticTransmissionLoss) {
            staticLights.push_back(lights[index]);
        } else {
            unobstructedLights.push_back(lights[index]);
        }
    }
    const sf::Vector2u mapSize = tilemap->getSize();
    const sf::Vector2f worldSize{
        static_cast<float>(mapSize.x * std::max(CellSize, 1)),
        static_cast<float>(mapSize.y * std::max(CellSize, 1)),
    };
    staticDirectLight->setView(sf::View(worldSize * 0.5f, worldSize));
    staticDirectLight->clear(sf::Color::Black);
    if (!staticLights.empty()) {
        setLightPassWorldUniforms();
        for (const ActiveLight& light : staticLights) {
            renderLight(light, {0.0f, 0.0f}, {0.0f, 0.0f}, true, false,
                        *staticDirectLight);
        }
    }
    renderUnobstructedLights(unobstructedLights, *staticDirectLight);
    staticDirectLight->display();
    cachedStaticLights = captureLights(lights);
    cachedStaticMaterialRevision = materialRevision;
    cachedStaticTransmissionGeneration = staticTransmissionGeneration;
}

std::tuple<sf::Vector2f, sf::Vector2f, bool>
GameMapRendererImpl::renderDynamicTransmission(
    const LightOcclusionResult& analysis) {
    if (!analysis.maskRect) {
        return {{0.0f, 0.0f}, {0.0f, 0.0f}, false};
    }
    const sf::Vector2f origin = analysis.maskRect->position;
    const sf::Vector2f size = analysis.maskRect->size;
    dynamicTransmission->setView(sf::View(origin + size * 0.5f, size));
    dynamicTransmission->clear(sf::Color::White);
    actorMaskShader->setUniform("transmissionMode", 1.0f);
    for (const std::shared_ptr<Actor>& actor : analysis.occluders) {
        if (!actor) {
            continue;
        }
        setActorMaskUniforms(*actor);
        dynamicTransmission->draw(*actor, transmissionActorStates);
    }
    dynamicTransmission->display();
    lightPassShader->setUniform("dynamicOccupancy", *analysis.dynamicOccupancy);
    lightPassShader->setUniform("dynamicOccupancyOrigin",
                                analysis.dynamicOccupancyOrigin);
    lightPassShader->setUniform("dynamicOccupancySize",
                                analysis.dynamicOccupancySize);
    return {origin, size, true};
}

const sf::Texture& GameMapRendererImpl::ensureStaticLightCache(
    std::size_t index, const ActiveLight& entry) {
    if (staticLightCaches.size() <= index) {
        staticLightCaches.resize(index + 1);
    }
    StaticLightCache& cache = staticLightCaches[index];
    const float diameter = entry.light.radius * 2.0f;
    const sf::Vector2u logicalSize{
        static_cast<unsigned int>(std::max(1.0f, std::ceil(diameter))),
        static_cast<unsigned int>(std::max(1.0f, std::ceil(diameter))),
    };
    const float scale = System::getLightingRenderScale();
    const sf::Vector2u requiredSize = lightingTargetSize(logicalSize, scale);
    const bool targetChanged =
        !cache.target || cache.target->getSize() != requiredSize;
    if (targetChanged) {
        cache.target = std::make_shared<sf::RenderTexture>(requiredSize);
    }
    cache.target->setSmooth(scale < 1.0f);
    if (!targetChanged && cache.valid &&
        cache.generation == staticTransmissionGeneration &&
        cache.light.matches(entry)) {
        return cache.target->getTexture();
    }
    cache.target->setView(sf::View(entry.light.position, {diameter, diameter}));
    cache.target->clear(sf::Color::Black);
    setLightPassCacheUniforms(*cache.target, entry.light);
    lightPassShader->setUniform("useCachedStaticLight", 0.0f);
    renderLight(entry, {0.0f, 0.0f}, {0.0f, 0.0f}, true, false, *cache.target);
    cache.target->display();
    cache.light = {entry.light, entry.owner.get()};
    cache.generation = staticTransmissionGeneration;
    cache.valid = true;
    return cache.target->getTexture();
}

void GameMapRendererImpl::setLightPassTextureUniforms() {
    lightPassShader->setUniform("staticTransmission",
                                staticTransmission->getTexture());
    lightPassShader->setUniform("staticOccupancy", *staticOccupancy);
    lightPassShader->setUniform("staticViewMode", 0.0f);
}

void GameMapRendererImpl::setLightPassCommonUniforms() {
    const sf::Vector2f screenSize = *camera->getViewSize();
    const sf::Vector2u targetSize = directLight->getSize();
    setLightPassTextureUniforms();
    lightPassShader->setUniform(
        "targetPixelScale",
        sf::Vector2f{static_cast<float>(targetSize.x) / screenSize.x,
                     static_cast<float>(targetSize.y) / screenSize.y});
    lightPassShader->setUniform("useCachedStaticLight", 0.0f);
    setViewShaderUniforms(*lightPassShader, screenSize, {0.0f, 0.0f}, true);
}

void GameMapRendererImpl::setLightPassWorldUniforms() {
    const sf::Vector2u mapSize = tilemap->getSize();
    const sf::Vector2f screenSize{
        static_cast<float>(mapSize.x * std::max(CellSize, 1)),
        static_cast<float>(mapSize.y * std::max(CellSize, 1)),
    };
    const sf::Vector2u targetSize = staticDirectLight->getSize();
    setLightPassTextureUniforms();
    lightPassShader->setUniform(
        "targetPixelScale",
        sf::Vector2f{static_cast<float>(targetSize.x) / screenSize.x,
                     static_cast<float>(targetSize.y) / screenSize.y});
    lightPassShader->setUniform("screenSize", screenSize);
    lightPassShader->setUniform("mapViewOffset", sf::Vector2f{});
    lightPassShader->setUniform("viewPos", sf::Vector2f{});
    lightPassShader->setUniform("viewSinCos", sf::Vector2f{0.0f, 1.0f});
    lightPassShader->setUniform("useCachedStaticLight", 0.0f);
    lightPassShader->setUniform("gridSize",
                                sf::Vector2f{static_cast<float>(mapSize.x),
                                             static_cast<float>(mapSize.y)});
    lightPassShader->setUniform("cellSize", static_cast<float>(CellSize));
}

void GameMapRendererImpl::setLightPassCacheUniforms(sf::RenderTexture& target,
                                                    const Light& light) {
    const float diameter = light.radius * 2.0f;
    const sf::Vector2u targetSize = target.getSize();
    const sf::Vector2u mapSize = tilemap->getSize();
    setLightPassTextureUniforms();
    lightPassShader->setUniform(
        "targetPixelScale",
        sf::Vector2f{static_cast<float>(targetSize.x) / diameter,
                     static_cast<float>(targetSize.y) / diameter});
    lightPassShader->setUniform("screenSize", sf::Vector2f{diameter, diameter});
    lightPassShader->setUniform("mapViewOffset", sf::Vector2f{});
    lightPassShader->setUniform(
        "viewPos", light.position - sf::Vector2f{light.radius, light.radius});
    lightPassShader->setUniform("viewSinCos", sf::Vector2f{0.0f, 1.0f});
    lightPassShader->setUniform("gridSize",
                                sf::Vector2f{static_cast<float>(mapSize.x),
                                             static_cast<float>(mapSize.y)});
    lightPassShader->setUniform("cellSize", static_cast<float>(CellSize));
}

void GameMapRendererImpl::setViewShaderUniforms(
    sf::Shader& shader, const sf::Vector2f& screenSize,
    const sf::Vector2f& mapViewOffset, bool usesFragmentCoordinates) const {
    if (usesFragmentCoordinates) {
        shader.setUniform("mapViewOffset", mapViewOffset);
    }
    shader.setUniform("screenSize", screenSize);
    shader.setUniform("viewPos", *camera->getViewPosition());
    const float radians = camera->getViewRotation().asRadians();
    shader.setUniform("viewSinCos",
                      sf::Vector2f{std::sin(radians), std::cos(radians)});
    const sf::Vector2u mapSize = tilemap->getSize();
    shader.setUniform("gridSize", sf::Vector2f{static_cast<float>(mapSize.x),
                                               static_cast<float>(mapSize.y)});
    shader.setUniform("cellSize", static_cast<float>(CellSize));
}

void GameMapRendererImpl::renderLight(const ActiveLight& entry,
                                      const sf::Vector2f& dynamicOrigin,
                                      const sf::Vector2f& dynamicSize,
                                      bool traceStatic, bool traceDynamic,
                                      sf::RenderTexture& target) {
    if (traceDynamic) {
        lightPassShader->setUniform("dynamicTransmission",
                                    dynamicTransmission->getTexture());
    }
    lightPassShader->setUniform("dynamicMaskOrigin", dynamicOrigin);
    lightPassShader->setUniform("dynamicMaskSize", dynamicSize);
    lightPassShader->setUniform("traceStatic", traceStatic ? 1.0f : 0.0f);
    lightPassShader->setUniform("traceDynamic", traceDynamic ? 1.0f : 0.0f);
    lightPassShader->setUniform("lightPos", entry.light.position);
    lightPassShader->setUniform("lightColor",
                                shaderColour(entry.light.colour, false));
    lightPassShader->setUniform("lightRadius", entry.light.radius);
    lightPassShader->setUniform("lightIntensity", entry.light.intensity);
    const float diameter = entry.light.radius * 2.0f;
    lightPassQuad.setSize({diameter, diameter});
    lightPassQuad.setPosition(
        entry.light.position -
        sf::Vector2f{entry.light.radius, entry.light.radius});
    target.draw(lightPassQuad, lightPassStates);
}

void GameMapRendererImpl::renderUnobstructedLights(
    const std::vector<ActiveLight>& lights, sf::RenderTexture& target) {
    if (lights.empty()) {
        unobstructedLightCache.clear();
        unobstructedLightCacheValid = false;
        return;
    }
    if (!unobstructedLightCacheValid ||
        !lightsMatch(lights, unobstructedLightCache)) {
        cacheUnobstructedLights(lights);
    }
    target.draw(unobstructedVertices, unobstructedLightStates);
}

void GameMapRendererImpl::cacheUnobstructedLights(
    const std::vector<ActiveLight>& lights) {
    unobstructedVertices.clear();
    for (std::size_t index = 0; index < lights.size(); ++index) {
        const Light& light = lights[index].light;
        unobstructedLightShader->setUniform(
            "lightIntensity[" + std::to_string(index) + "]", light.intensity);
        const float left = light.position.x - light.radius;
        const float right = light.position.x + light.radius;
        const float top = light.position.y - light.radius;
        const float bottom = light.position.y + light.radius;
        const sf::Color colour{light.colour.r, light.colour.g, light.colour.b,
                               static_cast<std::uint8_t>(index)};
        unobstructedVertices.append({{left, top}, colour, {-1.0f, -1.0f}});
        unobstructedVertices.append({{right, top}, colour, {1.0f, -1.0f}});
        unobstructedVertices.append({{right, bottom}, colour, {1.0f, 1.0f}});
        unobstructedVertices.append({{left, top}, colour, {-1.0f, -1.0f}});
        unobstructedVertices.append({{right, bottom}, colour, {1.0f, 1.0f}});
        unobstructedVertices.append({{left, bottom}, colour, {-1.0f, 1.0f}});
    }
    unobstructedLightCache = captureLights(lights);
    unobstructedLightCacheValid = true;
}
