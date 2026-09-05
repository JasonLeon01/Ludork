#pragma once

#include <GameMapBase.hpp>

#include <Camera.hpp>
#include <Gameplay/Actor.hpp>
#include <Gameplay/TileMap.hpp>

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class GameMapRendererImpl {
public:
    struct ActorState {
        Actor* actor = nullptr;
        const sf::Texture* texture = nullptr;
        sf::Vector2f position;
        sf::Vector2f translation;
        sf::Vector2f scale;
        sf::Vector2f origin;
        sf::Angle rotation;
        sf::IntRect textureRect;
        sf::Color colour;
        float lightBlock = 0.0f;
        bool mirror = false;
        float reflectionStrength = 0.0f;
        bool ignoreLighting = false;

        bool matches(const Actor& value, bool fullMaterial) const;
    };

    struct ActiveLight {
        Light light;
        std::shared_ptr<Actor> owner;
    };

    struct LightState {
        Light light;
        Actor* owner = nullptr;

        bool matches(const ActiveLight& value) const;
    };

    struct LayerMaskTextures {
        std::shared_ptr<sf::Image> lightBlockImage;
        std::shared_ptr<sf::Image> reflectionImage;
        std::shared_ptr<sf::Image> ignoreLightingImage;
        std::shared_ptr<sf::Texture> lightBlockTexture;
        std::shared_ptr<sf::Texture> reflectionTexture;
        std::shared_ptr<sf::Texture> ignoreLightingTexture;
    };

    struct StaticLightCache {
        std::shared_ptr<sf::RenderTexture> target;
        LightState light;
        std::size_t generation = 0;
        bool valid = false;
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
    void drawContent(
        sf::RenderTarget& target, const sf::RenderStates& states,
        bool applyPlayerCover, float shaderTime, int materialRevision,
        const std::function<void(const std::string&)>& drawLayerEffects);
    void drawActor(sf::RenderTarget& target, const sf::RenderStates& states,
                   const std::shared_ptr<Actor>& actor, int actorAlpha,
                   float shaderTime);
    void setActorEffectHidden(const std::shared_ptr<Actor>& actor, bool hidden);
    void resetTransparentTiles();

    void renderLighting(const std::vector<Light>& mapLights,
                        const sf::Color& ambientLight, int materialRevision);
    void refreshMaterialShader(const sf::Color& ambientLight);

    std::vector<std::shared_ptr<Actor>> renderSurfaceMask(int materialRevision);
    void rebuildStaticTransmission(
        const std::vector<std::shared_ptr<Actor>>& staticActors,
        int materialRevision);
    void setTileMaskUniforms(const std::string& cacheKey, TileLayer& layer);
    void setActorMaskUniforms(const Actor& actor);

    std::vector<ActiveLight> collectActiveLights(
        const std::vector<Light>& mapLights) const;
    std::pair<std::vector<std::shared_ptr<Actor>>,
              std::vector<std::shared_ptr<Actor>>>
    partitionLightBlockingActors(
        const std::vector<std::shared_ptr<Actor>>& visibleActors) const;
    void ensureDynamicTransmission(const std::vector<ActiveLight>& lights);
    bool ensureDirectLight();
    void ensureStaticDirectLight();
    void renderDynamicLighting(
        const std::vector<ActiveLight>& lights,
        const std::vector<LightOcclusionResult>& analyses);
    void renderCachedLighting(const std::vector<ActiveLight>& lights,
                              const std::vector<LightOcclusionResult>& analyses,
                              int materialRevision);
    std::tuple<sf::Vector2f, sf::Vector2f, bool> renderDynamicTransmission(
        const LightOcclusionResult& analysis);
    const sf::Texture& ensureStaticLightCache(std::size_t index,
                                              const ActiveLight& light);
    void renderLight(const ActiveLight& light,
                     const sf::Vector2f& dynamicOrigin,
                     const sf::Vector2f& dynamicSize, bool traceStatic,
                     bool traceDynamic, sf::RenderTexture& target);
    void renderUnobstructedLights(const std::vector<ActiveLight>& lights,
                                  sf::RenderTexture& target);
    void cacheUnobstructedLights(const std::vector<ActiveLight>& lights);
    void setLightPassTextureUniforms();
    void setLightPassCommonUniforms();
    void setLightPassWorldUniforms();
    void setLightPassCacheUniforms(sf::RenderTexture& target,
                                   const Light& light);
    void setViewShaderUniforms(sf::Shader& shader,
                               const sf::Vector2f& screenSize,
                               const sf::Vector2f& mapViewOffset,
                               bool usesFragmentCoordinates) const;

    bool staticTransmissionMatches(
        const std::vector<std::shared_ptr<Actor>>& actors,
        int materialRevision) const;
    bool surfaceMaskMatches(const std::vector<std::shared_ptr<Actor>>& actors,
                            int materialRevision) const;
    bool renderedLightingMatches(
        const std::vector<ActiveLight>& lights,
        const std::vector<std::shared_ptr<Actor>>& dynamicActors) const;
    void cacheRenderedLighting(
        const std::vector<ActiveLight>& lights,
        const std::vector<std::shared_ptr<Actor>>& dynamicActors);
    bool lightsMatch(const std::vector<ActiveLight>& lights,
                     const std::vector<LightState>& cache) const;
    std::vector<LightState> captureLights(
        const std::vector<ActiveLight>& lights) const;
    std::vector<ActorState> captureActors(
        const std::vector<std::shared_ptr<Actor>>& actors) const;
    std::vector<bool> captureLayerVisibility() const;

    int playerLayerIndex() const;
    bool preparePlayerCover(int layerIndex, int materialRevision,
                            sf::Vector2i& playerPosition);
    void applyPlayerCover(TileLayer& layer, int layerIndex,
                          int playerLayerIndex,
                          const sf::Vector2i& playerPosition);
    void drawLayerActors(sf::RenderTarget& target,
                         const sf::RenderStates& states,
                         const std::string& layerName, int layerIndex,
                         int playerLayerIndex, bool applyPlayerCover,
                         float shaderTime);
    bool drawActorShaderWithHue(sf::RenderTarget& target, Actor& actor,
                                sf::Shader& actorShader, float hue,
                                std::uint8_t actorAlpha);
    sf::RenderTexture& ensureActorShaderBuffer(const sf::Vector2u& size);
    sf::RenderTexture& ensureActorHueBuffer(const sf::Vector2u& size);
    sf::Sprite& ensureActorHueSourceSprite(const sf::Texture& texture);
    void applyActorHueUniform(float hue);

    static ActorState captureActor(const std::shared_ptr<Actor>& actor);
    static sf::Vector3f shaderColour(const sf::Color& colour, bool applyAlpha);
    static sf::Vector2u lightingTargetSize(const sf::Vector2u& size,
                                           float scale);
    static bool lightVisible(const Light& light,
                             const std::optional<sf::FloatRect>& viewport,
                             sf::Angle rotation);

    GameMapBase& map;
    std::shared_ptr<Tilemap> tilemap;
    std::shared_ptr<Camera> camera;
    std::vector<std::string> layerNames;
    std::uint8_t coverAlpha;
    bool previewOnly;

    std::shared_ptr<sf::Shader> materialShader;
    std::shared_ptr<sf::Shader> tileMaskShader;
    std::shared_ptr<sf::Shader> actorMaskShader;
    std::shared_ptr<sf::Shader> lightPassShader;
    std::shared_ptr<sf::Shader> unobstructedLightShader;
    std::shared_ptr<sf::Shader> actorHueShader;

    std::shared_ptr<sf::RenderTexture> staticTransmission;
    std::shared_ptr<sf::Texture> staticOccupancy;
    std::shared_ptr<sf::RenderTexture> surfaceMask;
    std::shared_ptr<sf::RenderTexture> dynamicTransmission;
    std::shared_ptr<sf::RenderTexture> directLight;
    std::shared_ptr<sf::RenderTexture> staticDirectLight;
    std::shared_ptr<sf::RenderTexture> actorShaderBuffer;
    std::shared_ptr<sf::RenderTexture> actorHueBuffer;
    std::optional<sf::Sprite> actorHueSourceSprite;

    sf::RenderStates surfaceTileStates;
    sf::RenderStates surfaceActorStates;
    sf::RenderStates transmissionTileStates;
    sf::RenderStates transmissionActorStates;
    sf::RenderStates lightPassStates;
    sf::RenderStates unobstructedLightStates;
    sf::RectangleShape lightPassQuad;
    sf::VertexArray unobstructedVertices{sf::PrimitiveType::Triangles};

    std::unordered_map<std::string, LayerMaskTextures> layerMaskTextures;
    std::unordered_set<Actor*> effectHiddenActors;
    std::vector<TransparentTile> transparentTiles;
    std::vector<std::pair<TileLayer*, bool>> coverLayers;
    std::optional<sf::Vector2i> coverPlayerPosition;
    int coverPlayerLayerIndex = -1;
    int coverMaterialRevision = -1;

    std::vector<ActorState> staticTransmissionActors;
    std::vector<ActorState> surfaceMaskActors;
    std::vector<bool> staticTransmissionLayers;
    std::vector<bool> surfaceMaskLayers;
    std::optional<sf::Vector2i> staticTransmissionCoverPosition;
    int staticTransmissionCoverLayer = -1;
    std::optional<sf::Vector2i> surfaceMaskCoverPosition;
    int surfaceMaskCoverLayer = -1;
    int staticTransmissionRevision = -1;
    int surfaceMaskRevision = -1;
    std::size_t staticTransmissionGeneration = 0;

    std::vector<StaticLightCache> staticLightCaches;
    std::vector<LightState> cachedStaticLights;
    int cachedStaticMaterialRevision = -1;
    std::size_t cachedStaticTransmissionGeneration = 0;
    std::vector<LightState> unobstructedLightCache;
    bool unobstructedLightCacheValid = false;

    std::vector<LightState> renderedLights;
    std::vector<ActorState> renderedDynamicActors;
    std::optional<sf::Vector2f> renderedViewPosition;
    std::optional<sf::Vector2f> renderedViewSize;
    sf::Angle renderedViewRotation;
    sf::Vector2u renderedTargetSize;
    std::size_t renderedStaticGeneration = 0;
    bool renderedLightingValid = false;

    unsigned int dynamicTransmissionSize = 0;
    bool directLightCleared = false;
    bool useStaticDirectLight = false;
};
