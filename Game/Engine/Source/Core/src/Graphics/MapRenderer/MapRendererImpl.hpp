#pragma once

#include <Graphics/MapRenderer.hpp>
#include <Gameplay/TileLayer.hpp>
#include <Gameplay/Tilemap/Tilemap.hpp>

#include <unordered_map>

class MapRenderer::Impl {
public:
    struct StaticLightCache {
        Light light;
        std::size_t generation = 0;
        std::uint64_t lastUse = 0;
        std::unique_ptr<sf::RenderTexture> texture;

        bool matches(const Light& value, sf::Vector2u size) const;
    };

    struct DynamicLightTexture {
        std::shared_ptr<sf::RenderTexture> transmission;
        std::shared_ptr<sf::Texture> occupancy;
    };

    struct DynamicLightCache {
        sf::Vector2u size;
        std::uint64_t lastUse = 0;
        DynamicLightTexture texture;
    };

    const sf::RenderTexture& render(const Scene& scene, const View& view,
                                    const Options& options,
                                    const DrawHooks& hooks,
                                    const sf::Texture* overlay);
    void drawContent(const Scene& scene, sf::RenderTarget& target,
                     const sf::RenderStates& states, const Options& options,
                     const DrawHooks& hooks);
    void drawSprite(const Sprite& sprite, sf::RenderTarget& target,
                    const sf::RenderStates& states, float time);
    void renderMasks(const Scene& scene, const View& view);
    void renderLighting(const Scene& scene, const View& view,
                        const Options& options);
    void renderTransmission(const Scene& scene, const View& view,
                            const std::vector<Light>& lights,
                            const Options& options);
    void drawSpriteMask(const Sprite& sprite, sf::RenderTarget& target,
                        bool transmission);
    void drawPanorama(const Scene& scene, const View& view,
                      const Options& options);
    void renderAmbient(const Scene& scene, const View& view,
                       const Options& options);
    void drawFog(const MapFogSettings& settings, const sf::FloatRect& bounds,
                 const Scene& scene, const View& view, const Options& options);
    std::shared_ptr<sf::Shader> shader(const std::string& path);
    sf::RenderTexture& target(std::unique_ptr<sf::RenderTexture>& value,
                              sf::Vector2u size);
    std::vector<std::size_t> prepareLightCaches(
        const std::vector<Light>& lights,
        const std::vector<sf::Vector2u>& sizes);
    DynamicLightTexture dynamicLightTexture(sf::Vector2u size,
                                            bool transmission);
    void trimDynamicLightingCache();
    void prepareOccupancy(sf::Vector2u size);
    void fillOccupancy(const sf::IntRect& bounds);
    std::shared_ptr<sf::Texture> createOccupancyTexture(sf::Vector2u size);
    static sf::View nativeView(const View& view);
    static sf::Vector3f colour(const sf::Color& value, bool alpha);
    static void setViewUniforms(sf::Shader& shader, const Scene& scene,
                                const View& view);
    static bool layerVisible(const Scene& scene, const Sprite& sprite);
    static std::vector<const Region*> orderedRegions(const Scene& scene);
    static void validate(const Scene& scene, const View& view,
                         const Options& options);

    Statistics statistics_;
    std::unique_ptr<sf::RenderTexture> content_;
    std::unique_ptr<sf::RenderTexture> output_;
    std::unique_ptr<sf::RenderTexture> surface_;
    std::unique_ptr<sf::RenderTexture> ambient_;
    std::unique_ptr<sf::RenderTexture> transmission_;
    std::unique_ptr<sf::RenderTexture> directLight_;

    std::unique_ptr<sf::RenderTexture> layerEffect_;
    std::unordered_map<std::string, std::unique_ptr<sf::RenderTexture>>
        layerCallbackBuffers_;
    std::unordered_map<std::string, std::shared_ptr<sf::Shader>> shaders_;
    ludork::engine::materials::MaterialProgramCache materialPrograms_;
    ludork::engine::materials::MaterialProgramCache::Context materialContext_;
    ludork::engine::materials::MaterialCompiler::Pass materialPass_ =
        ludork::engine::materials::MaterialCompiler::Pass::Color;
    bool hasReflection(const Scene& scene, const View& view) const;
    void drawOverlay(const sf::Texture& overlay, sf::RenderTarget& target,
                     const View& view);

    std::vector<StaticLightCache> lightCaches_;
    std::vector<DynamicLightCache> dynamicLightCaches_;
    std::uint64_t lightingCacheUse_ = 0;
    std::uint64_t dynamicLightingCacheBytes_ = 0;
    std::vector<std::uint8_t> occupancyPixels_;
    sf::Vector2u occupancyPixelSize_;
    std::shared_ptr<sf::Texture> occupancy_;
    sf::FloatRect transmissionRect_;
    bool transmissionValid_ = false;
    std::size_t transmissionSignature_ = 0;
    std::size_t transmissionGeneration_ = 0;
};
