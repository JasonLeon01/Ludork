#pragma once

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>
#include <PreviewSprite.hpp>

class Actor;
class Camera;
class FogController;
class GameMapBase;
class GameMapRendererImpl;
class Tilemap;

BIND_CLASS(metadata = false)
class LUDORK_GLOBAL_API GameMapRenderer {
public:
    BIND_INIT(allow_nil = "camera")
    GameMapRenderer(GameMapBase& map, std::shared_ptr<Tilemap> tilemap,
                    std::shared_ptr<Camera> camera,
                    const std::vector<std::string>& layerNames, int coverAlpha,
                    bool previewOnly);
    GameMapRenderer(const GameMapRenderer&) = delete;
    GameMapRenderer& operator=(const GameMapRenderer&) = delete;
    ~GameMapRenderer();

    BIND_METHOD(metadata = false)
    void setCamera(std::shared_ptr<Camera> camera);
    BIND_METHOD(metadata = false)
    void setFogController(std::shared_ptr<FogController> controller);
    BIND_METHOD(metadata = false)
    void setLayerOrder(const std::vector<std::string>& layers);
    BIND_METHOD(metadata = false)
    void setPreviewSprites(const std::vector<PreviewSprite>& sprites);
    BIND_METHOD(metadata = false)
    void setPreviewVisibility(const std::vector<bool>& visibility);
    BIND_METHOD(metadata = false)
    void setActorVisibilityFilter(
        std::function<bool(Actor&, const std::string&)> filter);
    BIND_METHOD(metadata = false, allow_nil = "drawLayerEffects")
    void drawContent(sf::RenderTarget& target, const sf::RenderStates& states,
                     bool applyPlayerCover, float shaderTime,
                     int materialRevision,
                     std::function<void(const std::string&)> drawLayerEffects);
    BIND_METHOD(metadata = false)
    void drawActor(sf::RenderTarget& target, const sf::RenderStates& states,
                   const std::shared_ptr<Actor>& actor, int actorAlpha,
                   float shaderTime);
    BIND_METHOD(metadata = false)
    void setActorEffectHidden(const std::shared_ptr<Actor>& actor, bool hidden);
    BIND_METHOD(metadata = false)
    void resetTransparentTiles();
    BIND_METHOD(metadata = false)
    void composeFrame(const sf::Color& ambientLight, float shaderTime);
    BIND_METHOD(metadata = false)
    void prewarm(const sf::Color& ambientLight, float shaderTime);

private:
    std::unique_ptr<GameMapRendererImpl> impl_;
};
