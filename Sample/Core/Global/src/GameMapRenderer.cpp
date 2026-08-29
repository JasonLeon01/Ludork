#include <GameMapRenderer.hpp>

#include "GameMapRenderer/Impl.hpp"

#include <Manager/ShaderManager.hpp>
#include <Runtime/EngineState.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

std::shared_ptr<sf::Shader> loadFragmentShader(const std::string& path) {
    return ShaderManager::load(path, sf::Shader::Type::Fragment);
}

}  // namespace

GameMapRenderer::GameMapRenderer(GameMapBase& map,
                                 std::shared_ptr<Tilemap> tilemap,
                                 std::shared_ptr<Camera> camera,
                                 const std::vector<std::string>& layerNames,
                                 int coverAlpha, bool previewOnly)
    : impl_(std::make_unique<GameMapRendererImpl>(map, std::move(tilemap),
                                                  std::move(camera), layerNames,
                                                  coverAlpha, previewOnly)) {}

GameMapRenderer::~GameMapRenderer() = default;

void GameMapRenderer::setCamera(std::shared_ptr<Camera> camera) {
    impl_->setCamera(std::move(camera));
}

void GameMapRenderer::drawContent(
    sf::RenderTarget& target, const sf::RenderStates& states,
    bool applyPlayerCover, float shaderTime, int materialRevision,
    std::function<void(const std::string&)> drawLayerEffects) {
    impl_->drawContent(target, states, applyPlayerCover, shaderTime,
                       materialRevision, drawLayerEffects);
}

void GameMapRenderer::drawActor(sf::RenderTarget& target,
                                const sf::RenderStates& states,
                                const std::shared_ptr<Actor>& actor,
                                int actorAlpha, float shaderTime) {
    impl_->drawActor(target, states, actor, actorAlpha, shaderTime);
}

void GameMapRenderer::setActorEffectHidden(const std::shared_ptr<Actor>& actor,
                                           bool hidden) {
    impl_->setActorEffectHidden(actor, hidden);
}

void GameMapRenderer::resetTransparentTiles() {
    impl_->resetTransparentTiles();
}

void GameMapRenderer::renderLighting(const std::vector<Light>& mapLights,
                                     const sf::Color& ambientLight,
                                     int materialRevision) {
    impl_->renderLighting(mapLights, ambientLight, materialRevision);
}

void GameMapRenderer::refreshMaterialShader(const sf::Color& ambientLight) {
    impl_->refreshMaterialShader(ambientLight);
}

std::shared_ptr<sf::Shader> GameMapRenderer::getMaterialShader() const {
    return impl_->materialShader;
}

GameMapRendererImpl::GameMapRendererImpl(
    GameMapBase& map, std::shared_ptr<Tilemap> tilemap,
    std::shared_ptr<Camera> camera, const std::vector<std::string>& layerNames,
    int coverAlpha, bool previewOnly)
    : map(map),
      tilemap(std::move(tilemap)),
      camera(std::move(camera)),
      layerNames(layerNames),
      coverAlpha(static_cast<std::uint8_t>(std::clamp(coverAlpha, 0, 255))),
      previewOnly(previewOnly) {
    if (!this->tilemap) {
        throw std::invalid_argument("GameMapRenderer tilemap must not be nil");
    }
    if (!sf::Shader::isAvailable()) {
        return;
    }
    actorHueShader = loadFragmentShader("Global/Hue.frag");
    if (previewOnly) {
        return;
    }
    if (!this->camera) {
        throw std::invalid_argument(
            "GameMapRenderer live camera must not be nil");
    }

    materialShader = loadFragmentShader("Global/Material.frag");
    tileMaskShader = loadFragmentShader("Global/TilemapLightMask.frag");
    actorMaskShader = loadFragmentShader("Global/LightMask.frag");
    lightPassShader = loadFragmentShader("Global/LightPass.frag");
    unobstructedLightShader =
        loadFragmentShader("Global/UnoccludedLightPass.frag");

    const sf::Vector2u mapSize = this->tilemap->getSize();
    const unsigned int cellSize =
        static_cast<unsigned int>(std::max(CellSize, 1));
    const sf::Vector2u maskSize{
        std::max(1u, mapSize.x * cellSize),
        std::max(1u, mapSize.y * cellSize),
    };
    staticTransmission = std::make_shared<sf::RenderTexture>(maskSize);
    staticTransmission->setSmooth(false);
    surfaceMask = std::make_shared<sf::RenderTexture>(maskSize);
    surfaceMask->setSmooth(false);

    const sf::RenderStates cameraStates = this->camera->getRenderStates();
    surfaceTileStates = sf::RenderStates(cameraStates.blendMode);
    surfaceTileStates.shader = tileMaskShader.get();
    surfaceActorStates = sf::RenderStates(cameraStates.blendMode);
    surfaceActorStates.shader = actorMaskShader.get();
    transmissionTileStates = sf::RenderStates(sf::BlendMultiply);
    transmissionTileStates.shader = tileMaskShader.get();
    transmissionActorStates = sf::RenderStates(sf::BlendMultiply);
    transmissionActorStates.shader = actorMaskShader.get();
    lightPassStates = sf::RenderStates(sf::BlendAdd);
    lightPassStates.shader = lightPassShader.get();
    lightPassQuad.setFillColor(sf::Color::White);
    unobstructedLightStates = sf::RenderStates(sf::BlendAdd);
    unobstructedLightStates.shader = unobstructedLightShader.get();
}

void GameMapRendererImpl::setCamera(std::shared_ptr<Camera> value) {
    camera = std::move(value);
    if (camera && tileMaskShader && actorMaskShader) {
        const sf::RenderStates cameraStates = camera->getRenderStates();
        surfaceTileStates = sf::RenderStates(cameraStates.blendMode);
        surfaceTileStates.shader = tileMaskShader.get();
        surfaceActorStates = sf::RenderStates(cameraStates.blendMode);
        surfaceActorStates.shader = actorMaskShader.get();
    }
    renderedLightingValid = false;
    directLight.reset();
}

sf::Vector3f GameMapRendererImpl::shaderColour(const sf::Color& colour,
                                               bool applyAlpha) {
    const float alpha =
        applyAlpha ? static_cast<float>(colour.a) / 255.0f : 1.0f;
    return {
        static_cast<float>(colour.r) / 255.0f * alpha,
        static_cast<float>(colour.g) / 255.0f * alpha,
        static_cast<float>(colour.b) / 255.0f * alpha,
    };
}

sf::Vector2u GameMapRendererImpl::lightingTargetSize(const sf::Vector2u& size,
                                                     float scale) {
    return {
        std::max(1u, static_cast<unsigned int>(
                         std::floor(static_cast<float>(size.x) * scale))),
        std::max(1u, static_cast<unsigned int>(
                         std::floor(static_cast<float>(size.y) * scale))),
    };
}
