#pragma once

#include <BindAnnotations.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

using LightValue = std::variant<std::vector<float>, std::vector<int>, float, int>;
using LightData = std::unordered_map<std::string, LightValue>;

////////////////////////////////////////////////////////////
/// \brief Light source with position, colour, radius, and intensity
///
////////////////////////////////////////////////////////////
BIND_CLASS(copyable = true)
struct Light {
    BIND_PROPERTY()
    sf::Vector2f position;

    BIND_PROPERTY()
    sf::Color colour;

    BIND_PROPERTY()
    float radius = 256.0f;

    BIND_PROPERTY()
    float intensity = 1.0f;

    ////////////////////////////////////////////////////////////
    /// \brief Construct a light source
    ///
    /// - \param position Light position in world space
    /// - \param colour Light colour
    /// - \param radius Light radius in pixels
    /// - \param intensity Light intensity multiplier
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    Light(const sf::Vector2f &position = sf::Vector2f(), const sf::Color &colour = sf::Color::White,
          float radius = 256.0f, float intensity = 1.0f);

    ////////////////////////////////////////////////////////////
    /// \brief Construct a light source from a pair position
    ///
    /// - \param position Light position as `(x, y)`
    /// - \param colour Light colour
    /// - \param radius Light radius in pixels
    /// - \param intensity Light intensity multiplier
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    Light(const std::pair<float, float> &position, const sf::Color &colour = sf::Color::White, float radius = 256.0f,
          float intensity = 1.0f);

    ////////////////////////////////////////////////////////////
    /// \brief Serialize the light to a dictionary-compatible map
    ///
    /// - \return Map containing position, color, radius, and intensity
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    LightData asDict() const;

    ////////////////////////////////////////////////////////////
    /// \brief Create a light from a dictionary-compatible map
    ///
    /// - \param data Raw light data
    /// - \return Light instance
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static Light fromDict(const LightData &data);
};
