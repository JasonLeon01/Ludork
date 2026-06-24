#pragma once

#include <BindAnnotations.hpp>
#include <General/AutoTile.hpp>
#include <General/Tileset.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

using TileGrid = std::vector<std::vector<std::optional<int>>>;
using AutoTileCell = std::optional<std::variant<int, std::string>>;
using AutoTileGrid = std::vector<std::vector<AutoTileCell>>;

////////////////////////////////////////////////////////////
/// \brief Tile layer data object.
///
/// Stores one layer's static tiles, autotile assignments and autotile
/// lookup pool. Runtime layers use integer autotile pool indices; editor
/// preview data may temporarily store autotile keys as strings.
////////////////////////////////////////////////////////////
BIND_CLASS(copyable=true)
struct TileLayerData {
    BIND_PROPERTY()
    std::string layerName = ""; ///< Layer name

    BIND_PROPERTY()
    Tileset layerTileset = Tileset(); ///< Tileset used by this layer

    BIND_PROPERTY()
    TileGrid tiles; ///< Static tile grid (`std::nullopt` for empty cells)

    BIND_PROPERTY()
    AutoTileGrid autoTiles; ///< Autotile grid (`std::nullopt` for empty cells)

    BIND_PROPERTY()
    std::vector<AutoTile> autoTilePool; ///< Autotile entries referenced by `autoTiles`

    BIND_PROPERTY()
    std::vector<std::string> autoTileKeys; ///< Data keys matching `autoTilePool`

    BIND_PROPERTY()
    std::string layerTilesetKey = ""; ///< Editor-side tileset data key

    BIND_PROPERTY()
    std::string shaderPath = ""; ///< Shader path relative to Assets/Shaders

    ////////////////////////////////////////////////////////////
    /// \brief Construct layer data
    ///
    /// - \param layerName Layer name
    /// - \param layerTileset Tileset used by this layer
    /// - \param tiles Static tile grid
    /// - \param autoTiles Autotile assignment grid
    /// - \param autoTilePool Autotile lookup pool
    /// - \param autoTileKeys Autotile data keys matching the pool
    /// - \param shaderPath Shader path relative to Assets/Shaders
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    TileLayerData(std::string layerName = "", Tileset layerTileset = Tileset(), TileGrid tiles = TileGrid(),
                  AutoTileGrid autoTiles = AutoTileGrid(), std::vector<AutoTile> autoTilePool = std::vector<AutoTile>(),
                  std::vector<std::string> autoTileKeys = std::vector<std::string>(), std::string shaderPath = "");
};
