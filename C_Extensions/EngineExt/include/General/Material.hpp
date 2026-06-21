#pragma once

#include <BindAnnotations.hpp>
#include <string>
#include <unordered_map>
#include <variant>

using MaterialValue = std::variant<bool, float>;
using MaterialData = std::unordered_map<std::string, MaterialValue>;

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
/// \brief Defines how an actor or tile interacts with
// lighting, rendering, and movement.
///
/// Applied per-actor or per-tile to control visual and
// gameplay properties.
////////////////////////////////////////////////////////////
BIND_CLASS(copyable = true)
struct Material {
    BIND_PROPERTY()
    float lightBlock = 0.0f;  ///< Amount of light blocked (0.0 = transparent, 1.0 = fully opaque)

    BIND_PROPERTY()
    bool mirror = false;  ///< Whether the surface reflects

    BIND_PROPERTY()
    float reflectionStrength = 0.5f;  ///< Intensity of reflection if mirrored

    BIND_PROPERTY()
    float opacity = 1.0f;  ///< Visual opacity (0.0 = invisible, 1.0 = fully visible)

    BIND_PROPERTY()
    float speedRate = 1.0f;  ///< Movement speed multiplier for actors on this surface

    ////////////////////////////////////////////////////////////
    /// \brief Construct a material object
    ///
    /// - \param inLightBlock Amount of light blocked (0.0 = transparent, 1.0 = fully opaque)
    /// - \param inMirror Whether the surface reflects
    /// - \param inReflectionStrength Intensity of reflection if mirrored
    /// - \param inOpacity Visual opacity (0.0 = invisible, 1.0 = fully visible)
    /// - \param inSpeedRate Movement speed multiplier for actors on this surface
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    Material(float lightBlock = 0.0f, bool mirror = false, float reflectionStrength = 0.5f, float opacity = 1.0f,
             float speedRate = 1.0f);

    ////////////////////////////////////////////////////////////
    /// \brief Serialize the material to a dictionary.
    ///
    /// - \return Dictionary containing all material fields
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    MaterialData asDict() const;

    ////////////////////////////////////////////////////////////
    /// \brief Create a material object from a raw data dictionary.
    ///
    /// - \param data Raw dictionary, e.g. loaded from JSON or .dat
    /// - \return The created material object
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static Material fromData(MaterialData data);
};
