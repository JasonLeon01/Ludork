#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

#include <Runtime/RuntimeData.hpp>

////////////////////////////////////////////////////////////
/// \brief Tileset data object.
///
/// Defines a tileset image and per-tile properties.
////////////////////////////////////////////////////////////
BIND_CLASS(copyable = true)
struct LUDORK_ENGINE_API Tileset {
    BIND_PROPERTY()
    std::string name = "";  ///< Tileset name

    BIND_PROPERTY(meta(PathVars = "/Game/Assets/Tilesets"))
    std::string fileName =
        "";  ///< Canonical texture path under /Game/Assets/Tilesets

    BIND_PROPERTY()
    std::vector<bool> passable;  ///< Per-tile passability

    BIND_PROPERTY()
    std::unordered_map<int, std::string>
        materials;  ///< Sparse material resource references by zero-based tile
                    ///< number

    BIND_PROPERTY()
    std::vector<std::array<bool, 4>>
        dir4;  ///< Per-tile 4-direction passability

    ////////////////////////////////////////////////////////////
    /// \brief Construct a tileset object
    ///
    /// - \param name Tileset name
    /// - \param fileName Canonical texture path under /Game/Assets/Tilesets
    /// - \param passable Per-tile passability values
    /// - \param materials Per-tile material values
    /// - \param dir4 Per-tile 4-direction passability values
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    Tileset(std::string name = "", std::string fileName = "",
            std::vector<bool> passable = std::vector<bool>(),
            std::unordered_map<int, std::string> materials = {},
            std::vector<std::array<bool, 4>> dir4 =
                std::vector<std::array<bool, 4>>());

    ////////////////////////////////////////////////////////////
    /// \brief Serialize the tileset to a dictionary.
    ///
    /// - \return Dictionary containing all tileset fields
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    RuntimeData::Map asDict() const;

    ////////////////////////////////////////////////////////////
    /// \brief Create a tileset object from a raw data dictionary.
    ///
    /// - \param data Raw dictionary, e.g. loaded from JSON or .dat
    /// - \return The created tileset object
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static Tileset fromData(const RuntimeData::Map& data);
};
