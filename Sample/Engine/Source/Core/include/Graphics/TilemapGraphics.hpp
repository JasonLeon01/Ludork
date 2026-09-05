#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>
#include <General/Material.hpp>
#include <General/TileLayerData.hpp>

#include <atomic>

namespace ludork::engine::tilemap_graphics_impl {
struct TileChunk;
}

////////////////////////////////////////////////////////////
/// \brief GPU-accelerated tile layer renderer using vertex arrays
///
/// Renders both static tile-id grids and RPG Maker XP-style autotile
/// grids. Geometry is stored in 32x32-cell chunks so draw submission can
/// skip chunks outside the current render-target view. Autotile cells consume
/// four 16x16 quadrants per 32x32 tile, composed from a 3-column by 4-row
/// mini-pattern selected via the 8-direction neighbour mask. Animation
/// advances the source frame on `updateAutoTileAnimation`.
///
////////////////////////////////////////////////////////////
BIND_CLASS()
class LUDORK_ENGINE_API TileLayerGraphics : public sf::Drawable,
                                            public sf::Transformable {
public:
    ////////////////////////////////////////////////////////////
    /// \brief Construct a tile layer graphics object
    ///
    /// - \param width Tile count on X axis
    /// - \param height Tile count on Y axis
    /// - \param tileSize Tile size in pixels
    /// - \param texture Tileset texture
    /// - \param data Tile layer data object
    /// - \param autoTileTextures Per-autotile-pool texture owners
    /// - \param autoTileFrameCounts Per-autotile-pool animation frame counts
    /// - \param deferred Defer geometry construction to `buildNextChunk`
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT(defaults = {false})
    TileLayerGraphics(
        int width, int height, int tileSize,
        std::shared_ptr<sf::Texture> texture, const TileLayerData& data,
        const std::vector<std::shared_ptr<sf::Texture>>& autoTileTextures,
        const std::vector<int>& autoTileFrameCounts, bool deferred = false);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~TileLayerGraphics();

    TileLayerGraphics(const TileLayerGraphics&) = delete;
    TileLayerGraphics& operator=(const TileLayerGraphics&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Set the color tint of a specific tile
    ///
    /// - \param x Tile X coordinate
    /// - \param y Tile Y coordinate
    /// - \param color Tint color to apply
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void setTileColor(int x, int y, sf::Color color);

    ////////////////////////////////////////////////////////////
    /// \brief Reset a tile color to its material opacity value
    ///
    /// - \param x Tile X coordinate
    /// - \param y Tile Y coordinate
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void resetTileColor(int x, int y);

    ////////////////////////////////////////////////////////////
    /// \brief Flood fill connected valid tiles with a color overlay
    ///
    /// - \param startX Seed tile X coordinate
    /// - \param startY Seed tile Y coordinate
    /// - \param color Fill color
    ///
    /// - \return List of affected tile coordinates
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<sf::Vector2i> floodFillTransparent(int startX, int startY,
                                                   sf::Color color);

    ////////////////////////////////////////////////////////////
    /// \brief Return the static tile index at a position
    ///
    /// - \param position Grid position
    ///
    /// - \return Tile index, or `std::nullopt`
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true)
    std::optional<int> get(const sf::Vector2i& position) const;

    ////////////////////////////////////////////////////////////
    /// \brief Return the autotile entry at a position
    ///
    /// - \param position Grid position
    ///
    /// - \return Autotile entry, or `std::nullopt`
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true)
    std::optional<AutoTile> getAutoTileAt(const sf::Vector2i& position) const;

    ////////////////////////////////////////////////////////////
    /// \brief Check whether one cell is passable
    ///
    /// - \param position Grid position
    ///
    /// - \return `true` if passable
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true)
    bool isPassable(const sf::Vector2i& position) const;

    ////////////////////////////////////////////////////////////
    /// \brief Return the material at a position
    ///
    /// - \param position Grid position
    ///
    /// - \return Material, or `std::nullopt`
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true)
    std::optional<Material> getMaterial(const sf::Vector2i& position) const;

    ////////////////////////////////////////////////////////////
    /// \brief Build a light-block grid
    ///
    /// - \return 2D light-block map
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<std::vector<float>> getLightBlockMap() const;

    ////////////////////////////////////////////////////////////
    /// \brief Build a reflection-strength grid
    ///
    /// - \return 2D reflection-strength map
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<std::vector<float>> getReflectionStrengthMap() const;

    ////////////////////////////////////////////////////////////
    /// \brief Build an ignore-lighting grid
    ///
    /// - \return 2D ignore-lighting map
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<std::vector<float>> getIgnoreLightingMap() const;

    ////////////////////////////////////////////////////////////
    /// \brief Advance autotile animation by an elapsed time
    ///
    /// Animation frames advance once per `frameInterval` seconds and the
    /// cached vertex texture coordinates are rewritten when the active
    /// frame changes.
    ///
    /// - \param deltaTime Elapsed time in seconds
    /// - \param frameInterval Seconds between two animation frames
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void updateAutoTileAnimation(float deltaTime, float frameInterval);

    ////////////////////////////////////////////////////////////
    /// \brief Return the unique chunks submitted by the latest draw
    ///
    /// - \return Submitted 32x32-cell chunk count
    ////////////////////////////////////////////////////////////
    BIND_METHOD(metadata = false)
    int getLastVisibleChunkCount() const;

    ////////////////////////////////////////////////////////////
    /// \brief Build the next 32x32-cell geometry chunk
    ///
    /// - \return `true` when every chunk is complete
    ////////////////////////////////////////////////////////////
    BIND_METHOD(metadata = false)
    bool buildNextChunk();

    ////////////////////////////////////////////////////////////
    /// \brief Build one geometry chunk by zero-based chunk coordinates
    ///
    /// - \param chunkX Chunk column
    /// - \param chunkY Chunk row
    /// - \return `true` when every chunk is complete
    ////////////////////////////////////////////////////////////
    BIND_METHOD(metadata = false)
    bool buildChunk(int chunkX, int chunkY);

    ////////////////////////////////////////////////////////////
    /// \brief Check whether one geometry chunk is complete
    ///
    /// - \param chunkX Chunk column
    /// - \param chunkY Chunk row
    /// - \return `true` when the chunk is complete
    ////////////////////////////////////////////////////////////
    BIND_METHOD(metadata = false)
    bool isChunkBuilt(int chunkX, int chunkY) const;

    bool isCellBuilt(const sf::Vector2i& position) const;

    ////////////////////////////////////////////////////////////
    /// \brief Check whether deferred geometry construction is complete
    ///
    /// - \return `true` when every chunk is complete
    ////////////////////////////////////////////////////////////
    BIND_METHOD(metadata = false)
    bool isBuildComplete() const;

protected:
    void writePendingBlock(int x, int y, const TileGrid& tileBlock,
                           const AutoTileGrid& autoTileBlock);

    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

private:
    static constexpr int ChunkSize = 32;

    ////////////////////////////////////////////////////////////
    /// \brief Create the fixed-size chunk index for this layer
    ///
    ////////////////////////////////////////////////////////////
    void initChunks();

    ////////////////////////////////////////////////////////////
    /// \brief Build all pending geometry chunks
    ////////////////////////////////////////////////////////////
    void buildAllChunks();

    ////////////////////////////////////////////////////////////
    /// \brief Build one static-tile geometry chunk
    ///
    /// - \param chunk Chunk to populate
    ///
    ////////////////////////////////////////////////////////////
    void buildStaticChunk(
        ludork::engine::tilemap_graphics_impl::TileChunk& chunk);

    ////////////////////////////////////////////////////////////
    /// \brief Build one autotile geometry chunk
    ///
    /// - \param chunk Chunk to populate
    ///
    ////////////////////////////////////////////////////////////
    void buildAutoTileChunk(
        ludork::engine::tilemap_graphics_impl::TileChunk& chunk);

    ////////////////////////////////////////////////////////////
    /// \brief Refresh vertex texture coordinates for one autotile pool
    ///
    /// - \param poolIndex Autotile pool index
    ///
    ////////////////////////////////////////////////////////////
    void refreshAutoTileTexCoords(int poolIndex);

    ////////////////////////////////////////////////////////////
    /// \brief Refresh one chunk for one autotile pool
    ///
    /// - \param chunk Chunk to update
    /// - \param poolIndex Autotile pool index
    ///
    ////////////////////////////////////////////////////////////
    void refreshAutoTileTexCoords(
        ludork::engine::tilemap_graphics_impl::TileChunk& chunk, int poolIndex);

    ////////////////////////////////////////////////////////////
    /// \brief Check whether a grid position is inside this layer
    ///
    /// - \param position Grid position
    ///
    /// - \return `true` if the position is valid
    ////////////////////////////////////////////////////////////
    bool inBounds(const sf::Vector2i& position) const;

    ////////////////////////////////////////////////////////////
    /// \brief Return an autotile pool index at a position
    ///
    /// - \param position Grid position
    ///
    /// - \return Pool index, or `std::nullopt`
    ////////////////////////////////////////////////////////////
    std::optional<int> getAutoTileIndex(const sf::Vector2i& position) const;

    ////////////////////////////////////////////////////////////
    /// \brief Return the chunk containing a tile position
    ///
    /// - \param x Tile X coordinate
    /// - \param y Tile Y coordinate
    ///
    /// - \return Owning chunk
    ////////////////////////////////////////////////////////////
    ludork::engine::tilemap_graphics_impl::TileChunk& getChunk(int x, int y);

    std::shared_ptr<sf::Texture> texture_;
    sf::Vector2f size_;
    int tileSize_;
    int chunkColumns_ = 0;
    int chunkRows_ = 0;
    std::vector<ludork::engine::tilemap_graphics_impl::TileChunk> chunks_;
    mutable std::atomic<int> lastVisibleChunkCount_{0};
    std::size_t nextBuildChunk_ = 0;
    std::size_t builtChunkCount_ = 0;
    std::vector<bool> builtChunks_;
    bool buildComplete_ = false;
    std::vector<std::vector<std::optional<int>>> tiles_;
    std::vector<bool> passable_;
    std::vector<Material> materials_;

    AutoTileGrid autoTiles_;
    std::vector<AutoTile> autoTilePool_;
    std::vector<std::shared_ptr<sf::Texture>> autoTileTextures_;
    std::vector<Material> autoTileMaterials_;
    std::vector<int> autoTileFrameCounts_;
    std::vector<int> autoTileCurrentFrames_;
    float autoTileAnimationAccum_;
};
