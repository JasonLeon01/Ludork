#pragma once

#include <BindAnnotations.hpp>
#include <General/Material.hpp>
#include <array>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using TilesetValue =
    std::variant<std::string, std::vector<bool>, std::vector<MaterialData>, std::vector<std::array<bool, 4>>>;
using TilesetData = std::unordered_map<std::string, TilesetValue>;

////////////////////////////////////////////////////////////
/// \brief Tileset data object.
///
/// Defines a tileset image and per-tile properties.
////////////////////////////////////////////////////////////
BIND_CLASS(copyable = true)
struct Tileset {
    BIND_PROPERTY()
    std::string name = "";  ///< Tileset name

    BIND_PROPERTY()
    std::string fileName = "";  ///< Texture image file name

    BIND_PROPERTY()
    std::vector<bool> passable;  ///< Per-tile passability

    BIND_PROPERTY()
    std::vector<Material> materials;  ///< Per-tile material values

    BIND_PROPERTY()
    std::vector<std::array<bool, 4>> dir4;  ///< Per-tile 4-direction passability

    ////////////////////////////////////////////////////////////
    /// \brief Construct a tileset object
    ///
    /// - \param name Tileset name
    /// - \param fileName Texture image file name
    /// - \param passable Per-tile passability values
    /// - \param materials Per-tile material values
    /// - \param dir4 Per-tile 4-direction passability values
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    Tileset(std::string name = "", std::string fileName = "", std::vector<bool> passable = std::vector<bool>(),
            std::vector<Material> materials = std::vector<Material>(),
            std::vector<std::array<bool, 4>> dir4 = std::vector<std::array<bool, 4>>());

    ////////////////////////////////////////////////////////////
    /// \brief Serialize the tileset to a dictionary.
    ///
    /// - \return Dictionary containing all tileset fields
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    TilesetData asDict() const;

    ////////////////////////////////////////////////////////////
    /// \brief Create a tileset object from a raw data dictionary.
    ///
    /// - \param data Raw dictionary, e.g. loaded from JSON or .dat
    /// - \return The created tileset object
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static Tileset fromData(TilesetData data);
};
