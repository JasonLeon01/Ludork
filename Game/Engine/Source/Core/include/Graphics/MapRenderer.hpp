#pragma once

#include <EngineRuntimeApi.hpp>
#include <General/Material.hpp>
#include <Graphics/MapFogSettings.hpp>
#include <SFML/Graphics.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Tilemap;

class LUDORK_ENGINE_API MapRenderer {
public:
    enum class LightingMode {
        Unlit,
        Lit
    };

    struct Region {
        std::string id;
        sf::Vector2i cellOrigin;
        std::shared_ptr<Tilemap> tilemap;
        std::uint64_t revision = 0;
        std::optional<MapFogSettings> fog;
        int priority = 0;
        std::optional<sf::Color> ambientLight;
    };

    struct Sprite {
        std::uint64_t id = 0;
        std::string layer;
        std::string regionId;
        std::shared_ptr<sf::Texture> texture;
        sf::IntRect textureRect;
        sf::Transform transform;
        sf::Color colour = sf::Color::White;
        Material material;
        std::shared_ptr<sf::Shader> shader;
        std::string shaderPath;
        float hue = 0.0f;
        float presentationOpacity = 1.0f;
        bool shaderError = false;
        bool visible = true;
        bool affectsLighting = true;
    };

    struct Light {
        sf::Vector2f position;
        sf::Color colour = sf::Color::White;
        float radius = 256.0f;
        float intensity = 1.0f;
        std::optional<std::uint64_t> emitterId;
    };

    struct Scene {
        sf::Vector2u cellSize;
        std::vector<std::string> layerOrder;
        std::vector<Region> regions;
        std::vector<Sprite> sprites;
        std::vector<Light> lights;
        sf::Color ambientLight = sf::Color::White;
        std::shared_ptr<sf::Texture> panorama;
        MapFogSettings fog;
    };

    struct View {
        sf::FloatRect worldRect;
        sf::Angle rotation;
        sf::Vector2u pixelSize;
    };

    struct Options {
        LightingMode lighting = LightingMode::Unlit;
        float time = 0.0f;
        float lightingScale = 1.0f;
        bool showFog = true;
        bool showPanorama = true;
        std::string activeLayer;
        float inactiveLayerOpacity = 1.0f;
    };

    struct DrawHooks {
        std::function<void(std::uint64_t, sf::RenderTarget&,
                           const sf::RenderStates&)>
            beforeSprite;
        std::function<void(std::uint64_t, sf::RenderTarget&,
                           const sf::RenderStates&)>
            afterSprite;
        std::function<void(const std::string&, sf::RenderTarget&,
                           const sf::RenderStates&)>
            afterLayer;
    };

    MapRenderer();
    ~MapRenderer();
    MapRenderer(const MapRenderer&) = delete;
    MapRenderer& operator=(const MapRenderer&) = delete;

    const sf::RenderTexture& render(const Scene& scene, const View& view,
                                    const Options& options);
    void drawContent(const Scene& scene, sf::RenderTarget& target,
                     const sf::RenderStates& states, const Options& options,
                     const DrawHooks& hooks = {});
    const sf::RenderTexture& compose(const Scene& scene, const View& view,
                                     const Options& options,
                                     const sf::Texture& content);
    void drawSprite(const Sprite& sprite, sf::RenderTarget& target,
                    const sf::RenderStates& states, float time);
    void reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
