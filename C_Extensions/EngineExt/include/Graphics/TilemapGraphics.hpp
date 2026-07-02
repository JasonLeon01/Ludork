#pragma once

#include <BindAnnotations.hpp>
#include <General/Material.hpp>
#include <General/TileLayerData.hpp>

#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Transformable.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>
#include <optional>
#include <string>
#include <vector>


////////////////////////////////////////////////////////////
/// \brief GPU-accelerated tile layer renderer using vertex arrays
///
/// Renders both static tile-id grids and RPG Maker XP-style autotile
/// grids. Autotile cells consume four 16x16 quadrants per 32x32 tile,
/// composed from a 3-column by 4-row mini-pattern selected via the
/// 8-direction neighbour mask. Each autotile pool entry owns its own
/// texture and vertex array; animation advances the source frame on
/// `updateAutoTileAnimation`.
///
////////////////////////////////////////////////////////////
BIND_CLASS()
class TileLayerGraphics : public sf::Drawable, public sf::Transformable {
public:
    ////////////////////////////////////////////////////////////
    /// \brief Construct a tile layer graphics object
    ///
    /// - \param width Tile count on X axis
    /// - \param height Tile count on Y axis
    /// - \param tileSize Tile size in pixels
    /// - \param texture Tileset texture
    /// - \param data Tile layer data object
    /// - \param autoTileTextures Per-autotile-pool texture pointers
    /// - \param autoTileFrameCounts Per-autotile-pool animation frame counts
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    TileLayerGraphics(int width, int height, int tileSize, sf::Texture *texture,
                      const TileLayerData &data,
                      const std::vector<sf::Texture *> &autoTileTextures,
                      const std::vector<int> &autoTileFrameCounts);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~TileLayerGraphics();

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
    std::vector<std::pair<int, int>> floodFillTransparent(int startX, int startY, sf::Color color);

    ////////////////////////////////////////////////////////////
    /// \brief Return the static tile index at a position
    ///
    /// - \param position Grid position
    ///
    /// - \return Tile index, or `std::nullopt`
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(tile="Optional[int]")
    BIND_METHOD()
    std::optional<int> get(const sf::Vector2i &position) const;

    ////////////////////////////////////////////////////////////
    /// \brief Return the autotile entry at a position
    ///
    /// - \param position Grid position
    ///
    /// - \return Autotile entry, or `std::nullopt`
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(autoTile="Optional[AutoTile]")
    BIND_METHOD()
    std::optional<AutoTile> getAutoTileAt(const sf::Vector2i &position) const;

    ////////////////////////////////////////////////////////////
    /// \brief Check whether one cell is passable
    ///
    /// - \param position Grid position
    ///
    /// - \return `true` if passable
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(isPassable="bool")
    BIND_METHOD()
    bool isPassable(const sf::Vector2i &position) const;

    ////////////////////////////////////////////////////////////
    /// \brief Return the material at a position
    ///
    /// - \param position Grid position
    ///
    /// - \return Material, or `std::nullopt`
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(material="Optional[Material]")
    BIND_METHOD()
    std::optional<Material> getMaterial(const sf::Vector2i &position) const;

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

private:
    ////////////////////////////////////////////////////////////
    /// \brief Build vertex data from tile and material tables
    ///
    /// - \param tileSize Tile size in pixels
    ///
    ////////////////////////////////////////////////////////////
    void init(int tileSize);

    ////////////////////////////////////////////////////////////
    /// \brief Build the autotile vertex arrays once the tile grid is known
    ///
    /// - \param tileSize Tile size in pixels
    ///
    ////////////////////////////////////////////////////////////
    void initAutoTiles(int tileSize);

    ////////////////////////////////////////////////////////////
    /// \brief Refresh vertex texture coordinates for one autotile pool
    ///
    /// - \param poolIndex Autotile pool index
    ///
    ////////////////////////////////////////////////////////////
    void refreshAutoTileTexCoords(int poolIndex);

    ////////////////////////////////////////////////////////////
    /// \brief Check whether a grid position is inside this layer
    ///
    /// - \param position Grid position
    ///
    /// - \return `true` if the position is valid
    ////////////////////////////////////////////////////////////
    bool inBounds(const sf::Vector2i &position) const;

    ////////////////////////////////////////////////////////////
    /// \brief Return an autotile pool index at a position
    ///
    /// - \param position Grid position
    ///
    /// - \return Pool index, or `std::nullopt`
    ////////////////////////////////////////////////////////////
    std::optional<int> getAutoTileIndex(const sf::Vector2i &position) const;

    ////////////////////////////////////////////////////////////
    /// \brief Draw tile vertices to a render target
    ///
    /// - \param target Destination render target
    /// - \param states Render state bundle
    ///
    ////////////////////////////////////////////////////////////
    virtual void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

    sf::VertexArray *vertexArray_;
    sf::Texture *texture_;
    sf::Vector2f size_;
    int tileSize_;
    TileLayerData data_;
    std::vector<std::vector<std::optional<int>>> tiles_;
    std::vector<Material> materials_;

    AutoTileGrid autoTiles_;
    std::vector<AutoTile> autoTilePool_;
    std::vector<sf::Texture *> autoTileTextures_;
    std::vector<Material> autoTileMaterials_;
    std::vector<int> autoTileFrameCounts_;
    std::vector<sf::VertexArray *> autoTileVertexArrays_;
    std::vector<std::vector<std::pair<int, int>>> autoTileCells_;
    std::vector<std::vector<int>> autoTileMasks_;
    std::vector<int> autoTileCurrentFrames_;
    float autoTileAnimationAccum_;
};
