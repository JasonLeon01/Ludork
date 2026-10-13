#pragma once

#include <GameMapBase.hpp>
#include <Camera.hpp>
#include <Gameplay/Actor.hpp>
#include <Gameplay/TileLayer.hpp>
#include <Gameplay/Tilemap/Tilemap.hpp>
#include <Graphics/MapRenderer.hpp>
#include <PreviewSprite.hpp>
#include <Fog/FogController.hpp>

class GameMapRendererImpl {
public:
    struct PreviewState {
        PreviewSprite data;
        sf::Transform transform;
        sf::FloatRect bounds;
        std::shared_ptr<sf::Shader> shader;
        bool shaderError = false;
        bool visibleOnMap = false;
    };
    struct TransparentTile {
        std::shared_ptr<TileLayer> layer;
        sf::Vector2i position;
    };

    GameMapRendererImpl(GameMapBase& map, std::shared_ptr<Tilemap> tilemap,
                        std::shared_ptr<Camera> camera,
                        const std::vector<std::string>& layerNames,
                        int coverAlpha, bool previewOnly);
    void setCamera(std::shared_ptr<Camera> value);
    void setPreviewSprites(const std::vector<PreviewSprite>& sprites);
    void setPreviewVisibility(const std::vector<bool>& visibility);
    void refreshPreviewVisibility();
    void prepareVisibleLayers();
    void drawContent(
        sf::RenderTarget& target, const sf::RenderStates& states,
        bool applyPlayerCover, float shaderTime, int materialRevision,
        const std::function<void(const std::string&)>& drawLayerEffects);
    void drawActor(sf::RenderTarget& target, const sf::RenderStates& states,
                   const std::shared_ptr<Actor>& actor, int actorAlpha,
                   float shaderTime);
    void composeFrame(const sf::Color& ambientLight, float shaderTime);
    void prewarm(const sf::Color& ambientLight, float shaderTime);
    void buildScene(bool applyPlayerCover, int materialRevision);
    MapRenderer::Sprite actorSprite(const std::shared_ptr<Actor>& actor,
                                    const std::string& layer, int alpha) const;
    MapRenderer::View renderView() const;
    int playerLayerIndex() const;
    bool preparePlayerCover(int layerIndex, int materialRevision,
                            sf::Vector2i& position);
    void resetTransparentTiles();
    void applyPlayerCover(TileLayer& layer, int layerIndex, int playerLayer,
                          const sf::Vector2i& playerPosition);

    GameMapBase& map;
    std::shared_ptr<Tilemap> tilemap;
    std::shared_ptr<Tilemap> sourceTilemap;
    std::shared_ptr<Camera> camera;
    std::vector<std::string> layerNames;
    std::uint8_t coverAlpha;
    bool previewOnly;
    bool previewSpritesInstalled = false;
    std::optional<std::size_t> displayVisibilityRevision;
    std::vector<PreviewState> previewSprites;
    std::unordered_set<Actor*> effectHiddenActors;
    std::function<bool(Actor&, const std::string&)> actorVisibilityFilter;
    std::unordered_map<std::uint64_t, std::shared_ptr<Actor>> actorOwners;
    std::shared_ptr<FogController> fogController;
    MapRenderer renderer;
    MapRenderer::Scene scene;
    std::vector<TransparentTile> transparentTiles;
    std::vector<std::pair<TileLayer*, bool>> coverLayers;
    std::optional<sf::Vector2i> coverPlayerPosition;
    int coverPlayerLayerIndex = -1;
    int coverMaterialRevision = -1;
};
