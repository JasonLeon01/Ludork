#pragma once

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

// BIND_CLASS
// GPU-accelerated tile layer renderer using vertex arrays.
class TileLayerGraphics : public sf::Drawable, public sf::Transformable {
public:
    // BIND_INIT
    TileLayerGraphics(int width, int height, int tileSize, sf::Texture *texture,
                      const std::vector<std::vector<std::optional<int>>> &tiles,
                      const std::vector<py::object> &materials);
    ~TileLayerGraphics();

    // BIND_METHOD
    // Set the colour tint of a specific tile.
    void setTileColor(int x, int y, sf::Color color);

    // BIND_METHOD
    // Reset a tile's colour to its default opacity-based value.
    void resetTileColor(int x, int y);

    // BIND_METHOD
    // Flood-fill connected tiles with a transparent colour overlay.
    // Returns the list of affected tile coordinates.
    std::vector<std::pair<int, int>> floodFillTransparent(int startX, int startY, sf::Color color);

private:
    void init(int tileSize);
    virtual void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

    sf::VertexArray *vertexArray_;
    sf::Texture *texture_;
    sf::Vector2f size_;
    std::vector<std::vector<std::optional<int>>> tiles_;
    std::vector<py::object> materials_;
};

void ApplyTileLayerGraphicsBinding(py::module &m);
