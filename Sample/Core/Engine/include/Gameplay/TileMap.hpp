#pragma once

#include <EngineRuntimeApi.hpp>

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <General/Material.hpp>
#include <General/TileLayerData.hpp>
#include <Graphics/TilemapGraphics.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API TileLayer : public TileLayerGraphics {
public:
    BIND_INIT(defaults = {{}, {}, true, false})
    TileLayer(
        const TileLayerData& data, std::shared_ptr<sf::Texture> texture,
        const std::vector<std::shared_ptr<sf::Texture>>& autoTileTextures =
            std::vector<std::shared_ptr<sf::Texture>>(),
        const std::vector<int>& autoTileFrameCounts = std::vector<int>(),
        bool visible = true, bool deferred = false);
    ~TileLayer() override;

    BIND_METHOD(Pure = true, returns = "name")
    virtual const std::string& getName() const;

    BIND_METHOD(Pure = true, returns = "tiles")
    const TileGrid& getTiles() const;

    BIND_METHOD(Pure = true, returns = "autoTiles")
    const AutoTileGrid& getAutoTiles() const;

    BIND_METHOD(Pure = true, returns = "pool")
    const std::vector<AutoTile>& getAutoTilePool() const;

    BIND_METHOD(metadata = false)
    std::optional<std::string> getAutoTileKey(int poolIndex) const;

    BIND_METHOD(metadata = false)
    TileLayerData getData() const;

    BIND_METHOD(metadata = false)
    void writeBlock(int x, int y, const TileGrid& tileBlock,
                    const AutoTileGrid& autoTileBlock);

    BIND_METHOD(metadata = false)
    std::vector<std::shared_ptr<sf::Texture>> getAutoTileTextures() const;

    BIND_METHOD(metadata = false)
    std::vector<int> getAutoTileFrameCounts() const;

    BIND_METHOD(metadata = false)
    std::shared_ptr<TileLayer> rebuild(
        const TileLayerData& data,
        const std::vector<std::shared_ptr<sf::Texture>>& autoTileTextures,
        const std::vector<int>& autoTileFrameCounts) const;

    BIND_METHOD(Pure = true)
    virtual bool getVisible() const;

    BIND_METHOD()
    virtual void setVisible(bool visible);

    BIND_METHOD(Pure = true)
    virtual bool hasContent(const sf::Vector2i& position) const;

    BIND_METHOD(Pure = true)
    virtual bool isDirectionPassable(const sf::Vector2i& position,
                                     int directionIndex) const;

    BIND_METHOD(Pure = true)
    virtual std::optional<MaterialValue> getMaterialProperty(
        const sf::Vector2i& position, const std::string& propertyName) const;

    BIND_METHOD(Pure = true, returns = "lightBlock")
    std::optional<float> getLightBlock(const sf::Vector2i& position) const;

    BIND_METHOD(Pure = true, returns = "mirror")
    std::optional<bool> getMirror(const sf::Vector2i& position) const;

    BIND_METHOD(Pure = true, returns = "reflectionStrength")
    std::optional<float> getReflectionStrength(
        const sf::Vector2i& position) const;

    BIND_METHOD(Pure = true, returns = "ignoreLighting")
    std::optional<bool> getIgnoreLighting(const sf::Vector2i& position) const;

    BIND_METHOD(Pure = true, returns = "speedRate")
    std::optional<float> getSpeedRate(const sf::Vector2i& position) const;

    BIND_METHOD()
    virtual void updateShader(float deltaTime);

    BIND_METHOD(Pure = true)
    std::shared_ptr<sf::Shader> getShader() const;

    BIND_METHOD(Pure = true)
    std::shared_ptr<sf::Image> getLightBlockImage();

    BIND_METHOD(Pure = true)
    std::shared_ptr<sf::Image> getReflectionStrengthImage();

    BIND_METHOD(Pure = true)
    std::shared_ptr<sf::Image> getIgnoreLightingImage();

    BIND_METHOD(Pure = true)
    sf::Vector2u getGridSize() const;

    BIND_METHOD(metadata = false)
    bool isCellBuilt(const sf::Vector2i& position) const;

    const std::vector<std::vector<float>>& getLightBlockMapView();

    BIND_PROPERTY()
    bool visible = true;

    BIND_PROPERTY(meta(PathVars = "/Game/Assets/Shaders",
                       PathFilter = "*.frag"))
    std::string shaderPath;

    BIND_PROPERTY(metadata = false)
    std::shared_ptr<sf::Shader> shader;

private:
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    static int layerWidth(const TileLayerData& data);
    static int layerHeight(const TileLayerData& data);
    static const std::shared_ptr<sf::Texture>& requireTexture(
        const std::shared_ptr<sf::Texture>& texture);
    static std::vector<std::shared_ptr<sf::Texture>> normalizeAutoTileTextures(
        const TileLayerData& data,
        const std::vector<std::shared_ptr<sf::Texture>>& textures);
    static std::vector<int> normalizeFrameCounts(
        const TileLayerData& data, const std::vector<int>& frameCounts);
    static std::optional<float> materialFloat(
        const std::optional<MaterialValue>& value);
    static std::optional<bool> materialBool(
        const std::optional<MaterialValue>& value);

    void loadShader();
    std::shared_ptr<sf::Image> buildMaterialImage(
        const std::vector<std::vector<float>>& values) const;

    TileLayerData data_;
    int width_ = 0;
    int height_ = 0;
    std::shared_ptr<sf::Texture> texture_;
    std::vector<std::shared_ptr<sf::Texture>> autoTileTextures_;
    std::vector<int> autoTileFrameCounts_;
    float shaderTime_ = 0.0f;
    bool shaderUsesTime_ = false;
    std::optional<std::vector<std::vector<float>>> lightBlockMapCache_;
    std::optional<std::vector<std::vector<float>>> reflectionStrengthMapCache_;
    std::optional<std::vector<std::vector<float>>> ignoreLightingMapCache_;
    std::shared_ptr<sf::Image> lightBlockImageCache_;
    std::shared_ptr<sf::Image> reflectionStrengthImageCache_;
    std::shared_ptr<sf::Image> ignoreLightingImageCache_;
};

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API Tilemap {
public:
    BIND_INIT()
    explicit Tilemap(const std::vector<std::shared_ptr<TileLayer>>& layers);
    virtual ~Tilemap();

    BIND_METHOD()
    void addLayer(const std::shared_ptr<TileLayer>& layer);

    BIND_METHOD(Pure = true, returns = "layer")
    virtual std::shared_ptr<TileLayer> getLayer(const std::string& name) const;

    BIND_METHOD(Pure = true, returns = "tiles")
    std::unordered_map<std::string, TileGrid> getTilesData() const;

    BIND_METHOD(Pure = true, returns = "layers")
    virtual std::unordered_map<std::string, std::shared_ptr<TileLayer>>
    getAllLayers() const;

    BIND_METHOD(Pure = true, returns = "layerNames")
    virtual std::vector<std::string> getLayerNameList() const;

    BIND_METHOD(Pure = true, returns = "autoTiles")
    std::unordered_map<std::string, AutoTileGrid> getAutoTilesData() const;

    BIND_METHOD(Pure = true, returns = "size")
    virtual sf::Vector2u getSize() const;

    BIND_METHOD()
    void updateAutoTileAnimation(float deltaTime, float frameInterval = 0.5f);

private:
    std::unordered_map<std::string, std::shared_ptr<TileLayer>> layers_;
    std::vector<std::string> layerNames_;
};
