#pragma once

#include <BindAnnotations.hpp>

#include <pybind11/pybind11.h>

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


namespace py = pybind11;

////////////////////////////////////////////////////////////
/// \brief GPU-accelerated tile layer renderer using vertex arrays
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
    /// - \param tiles Tile id grid
    /// - \param materials Material metadata list
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    TileLayerGraphics(int width, int height, int tileSize, sf::Texture *texture,
                      const std::vector<std::vector<std::optional<int>>> &tiles,
                      const std::vector<py::object> &materials);

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

private:
    ////////////////////////////////////////////////////////////
    /// \brief Build vertex data from tile and material tables
    ///
    /// - \param tileSize Tile size in pixels
    ///
    ////////////////////////////////////////////////////////////
    void init(int tileSize);

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
    std::vector<std::vector<std::optional<int>>> tiles_;
    std::vector<py::object> materials_;
};

////////////////////////////////////////////////////////////
/// \brief Register `TileLayerGraphics` bindings to Python
///
/// - \param m Target Python module
///
////////////////////////////////////////////////////////////
void ApplyTileLayerGraphicsBinding(py::module &m);
