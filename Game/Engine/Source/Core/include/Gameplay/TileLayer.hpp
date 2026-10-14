#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <General/TileLayerData.hpp>
#include <Graphics/TilemapGraphics.hpp>

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API TileLayer : public TileLayerGraphics {
public:
    LUDORK_CAST_DERIVED(TileLayer, TileLayerGraphics)

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

    void updateBlock(int x, int y, const TileGrid& tileBlock,
                     const AutoTileGrid& autoTileBlock);
    void setAnimationTime(float seconds, float frameInterval = 0.5f);

    BIND_METHOD(metadata = false)
    std::vector<std::shared_ptr<sf::Texture>> getAutoTileTextures() const;

    BIND_METHOD(metadata = false)
    std::vector<int> getAutoTileFrameCounts() const;

    BIND_METHOD(metadata = false)
    std::shared_ptr<TileLayer> rebuild(
        const TileLayerData& data,
        const std::vector<std::shared_ptr<sf::Texture>>& autoTileTextures,
        const std::vector<int>& autoTileFrameCounts) const;

    std::size_t getContentRevision() const;
    std::shared_ptr<TileLayer> createDisplayLayer(
        const std::vector<std::vector<sf::Vector2i>>& sources) const;
    void syncDisplayAnimation(const TileLayer& source);

    BIND_METHOD(Pure = true)
    virtual bool getVisible() const;

    BIND_METHOD()
    virtual void setVisible(bool visible);

    BIND_METHOD(Pure = true)
    virtual bool hasContent(const sf::Vector2i& position) const;

    BIND_METHOD(Pure = true)
    virtual bool isDirectionPassable(const sf::Vector2i& position,
                                     int directionIndex) const;

    std::shared_ptr<const ludork::engine::materials::MaterialResource>
    getLayerMaterial() const;
    void drawMaterialPass(
        sf::RenderTarget& target, sf::RenderStates states,
        ludork::engine::materials::MaterialProgramCache& programs,
        ludork::engine::materials::MaterialCompiler::Pass pass,
        ludork::engine::materials::MaterialProgramCache::Context context) const;
    bool hasReflectiveMaterial(const sf::FloatRect& bounds) const;
    sf::Vector2f maximumMaterialOffset() const;
    bool hasAnimatedOcclusion() const;

    BIND_METHOD(Pure = true)
    sf::Vector2u getGridSize() const;

    BIND_METHOD(metadata = false)
    bool isCellBuilt(const sf::Vector2i& position) const;

    BIND_PROPERTY()
    bool visible = true;

    BIND_PROPERTY(meta(PathRoot = "Data", PathVars = "Data/Materials",
                       PathFilter = "*.json"))
    std::string material;

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
    void invalidateContent();

    TileLayerData data_;
    std::size_t contentRevision_ = 0;
    int width_ = 0;
    int height_ = 0;
    std::shared_ptr<sf::Texture> texture_;
    std::vector<std::shared_ptr<sf::Texture>> autoTileTextures_;
    std::vector<int> autoTileFrameCounts_;
    float materialTime_ = 0.0f;
    mutable ludork::engine::materials::MaterialProgramCache materialPrograms_;
};
