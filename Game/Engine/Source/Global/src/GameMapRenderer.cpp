#include <GameMapRenderer.hpp>
#include "GameMapRenderer/GameMapRendererImpl.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

GameMapRenderer::GameMapRenderer(GameMapBase& map,
                                 std::shared_ptr<Tilemap> tilemap,
                                 std::shared_ptr<Camera> camera,
                                 const std::vector<std::string>& layers,
                                 int coverAlpha, bool previewOnly)
    : impl_(std::make_unique<GameMapRendererImpl>(map, std::move(tilemap),
                                                  std::move(camera), layers,
                                                  coverAlpha, previewOnly)) {}
GameMapRenderer::~GameMapRenderer() = default;
void GameMapRenderer::setCamera(std::shared_ptr<Camera> camera) {
    impl_->setCamera(std::move(camera));
}
void GameMapRenderer::setFogController(
    std::shared_ptr<FogController> controller) {
    impl_->fogController = std::move(controller);
}
void GameMapRenderer::setLayerOrder(const std::vector<std::string>& layers) {
    impl_->layerNames = layers;
}
void GameMapRenderer::setPreviewSprites(
    const std::vector<PreviewSprite>& sprites) {
    impl_->setPreviewSprites(sprites);
}
void GameMapRenderer::setPreviewVisibility(
    const std::vector<bool>& visibility) {
    impl_->setPreviewVisibility(visibility);
}
void GameMapRenderer::setActorVisibilityFilter(
    std::function<bool(Actor&, const std::string&)> filter) {
    impl_->actorVisibilityFilter = std::move(filter);
}
void GameMapRenderer::drawContent(
    sf::RenderTarget& target, const sf::RenderStates& states, bool cover,
    float time, int revision,
    std::function<void(const std::string&, sf::RenderTarget&,
                       const sf::RenderStates&)>
        effects) {
    impl_->drawContent(target, states, cover, time, revision, effects);
}
void GameMapRenderer::drawActor(sf::RenderTarget& target,
                                const sf::RenderStates& states,
                                const std::shared_ptr<Actor>& actor, int alpha,
                                float time) {
    impl_->drawActor(target, states, actor, alpha, time);
}
void GameMapRenderer::setActorEffectHidden(const std::shared_ptr<Actor>& actor,
                                           bool hidden) {
    if (!actor) {
        return;
    }
    if (hidden) {
        impl_->effectHiddenActors.insert(actor.get());
    } else {
        impl_->effectHiddenActors.erase(actor.get());
    }
}
void GameMapRenderer::resetTransparentTiles() {
    impl_->resetTransparentTiles();
}
void GameMapRenderer::renderFrame(
    const sf::Color& ambient, float time, bool cover, int revision,
    std::function<void(const std::string&, sf::RenderTarget&,
                       const sf::RenderStates&)>
        effects,
    std::function<void()> overlays) {
    impl_->renderFrame(ambient, time, cover, revision, effects, overlays);
}
void GameMapRenderer::prewarm(const sf::Color& ambient, float time) {
    impl_->prewarm(ambient, time);
}

GameMapRendererImpl::GameMapRendererImpl(GameMapBase& map,
                                         std::shared_ptr<Tilemap> tilemap,
                                         std::shared_ptr<Camera> camera,
                                         const std::vector<std::string>& layers,
                                         int coverAlpha, bool previewOnly)
    : map(map),
      tilemap(std::move(tilemap)),
      sourceTilemap(this->tilemap),
      camera(std::move(camera)),
      layerNames(layers),
      coverAlpha(static_cast<std::uint8_t>(std::clamp(coverAlpha, 0, 255))),
      previewOnly(previewOnly) {
    if (!this->tilemap) {
        throw std::invalid_argument("GameMapRenderer tilemap must not be nil");
    }
}
void GameMapRendererImpl::setCamera(std::shared_ptr<Camera> value) {
    camera = std::move(value);
}
