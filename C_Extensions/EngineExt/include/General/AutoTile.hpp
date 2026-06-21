#pragma once

#include <BindAnnotations.hpp>
#include <General/Material.hpp>
#include <string>
#include <unordered_map>
#include <variant>

using AutoTileValue = std::variant<std::string, bool, MaterialData>;
using AutoTileData = std::unordered_map<std::string, AutoTileValue>;

////////////////////////////////////////////////////////////
/// \brief AutoTile dataclass.

/// Defines an auto-tiling tile entry. Unlike `Tileset`, an `AutoTile`
/// represents a single logical tile whose appearance is derived from a
/// source image laid out in the standard 3-columns x 4-rows mini-tile
/// pattern. The image width may be a multiple of 3 cells when the
/// auto-tile is animated (one mini-pattern per animation frame).
///
/// Each `AutoTile` carries a single passability flag and a single
/// material because the whole entry behaves as one tile in gameplay.
////////////////////////////////////////////////////////////
BIND_CLASS(copyable = true)
struct AutoTile {
    BIND_PROPERTY()
    std::string name = "";  ///< AutoTile name

    BIND_PROPERTY()
    std::string fileName = "";  ///< Texture image file name (relative to Assets/Autotiles)

    BIND_PROPERTY()
    bool passable = true;  ///< Whether the auto-tile can be walked on

    BIND_PROPERTY()
    Material material = Material();  ///< Material applied to the whole auto-tile

    ////////////////////////////////////////////////////////////
    /// \brief Construct an auto-tile object
    ///
    /// - \param inName AutoTile name
    /// - \param inFileName Texture image file name (relative to Assets/Autotiles)
    /// - \param inPassable Whether the auto-tile can be walked on
    /// - \param inMaterial Material applied to the whole auto-tile
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    AutoTile(std::string name = "", std::string fileName = "", bool passable = true, Material material = Material());

    ////////////////////////////////////////////////////////////
    /// \brief Serialize the material to a dictionary.
    ///
    /// - \return Dictionary containing all material fields
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    AutoTileData asDict() const;

    ////////////////////////////////////////////////////////////
    /// \brief Create an auto-tile object from a raw data dictionary.
    ///
    /// - \param data Raw dictionary, e.g. loaded from JSON or .dat
    /// - \return The created auto-tile object
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static AutoTile fromData(AutoTileData data);
};
