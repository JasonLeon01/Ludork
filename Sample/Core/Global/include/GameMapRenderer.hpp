#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Light.hpp>

#include <SFML/Graphics.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Actor;
class Camera;
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
    GameMapRenderer(GameMapRenderer&&) = delete;
    GameMapRenderer& operator=(GameMapRenderer&&) = delete;
    ~GameMapRenderer();

    BIND_METHOD(metadata = false)
    void setCamera(std::shared_ptr<Camera> camera);

    BIND_METHOD(metadata = false)
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
    void renderLighting(const std::vector<Light>& mapLights,
                        const sf::Color& ambientLight, int materialRevision);

    BIND_METHOD(metadata = false)
    void refreshMaterialShader(const sf::Color& ambientLight);

    BIND_METHOD(metadata = false)
    std::shared_ptr<sf::Shader> getMaterialShader() const;

private:
    std::unique_ptr<GameMapRendererImpl> impl_;
};
