#pragma once
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace py = pybind11;
using ActorDict = std::unordered_map<std::string, std::vector<py::object>>;
using TileGrids = std::vector<std::vector<std::optional<int>>>;
using IntPair = std::pair<int, int>;

// BIND_CLASS
// C++ accelerated game map providing lighting, pathfinding and material processing.
class GameMapExt {
public:
    GameMapExt() = delete;

    // BIND_INIT
    GameMapExt(sf::Shader *shader);

    // BIND_METHOD
    // Refresh the material shader uniforms with current lighting data.
    void refreshShader(const sf::RenderTexture &lightMask, float screenScale, const sf::Vector2f &screenSize,
                       const sf::Vector2f &viewPos, float viewRot, const sf::Vector2f &gridSize, int cellSize,
                       const std::vector<py::object> &lights, const sf::Color &ambientColor);

    // BIND_METHOD
    // Generate a material data texture from the map.
    sf::Texture *generateDataFromMap(const sf::Vector2u &size, const std::vector<std::vector<float>> &materialMap,
                                     bool smooth = false);

    // BIND_METHOD
    // Perform A* pathfinding between two grid positions.
    std::vector<sf::Vector2i> findPathExt(const sf::Vector2i &start, const sf::Vector2i &goal,
                                          const sf::Vector2u &size);

    // BIND_METHOD
    // Build a 2D grid of material property values.
    std::vector<std::vector<py::object>> getMaterialPropertyMapExt(int width, int height,
                                                                   const std::string &functionName,
                                                                   const py::object &invalidValue);

    // BIND_METHOD
    // Rebuild the tile passability cache and actor occupancy map.
    std::pair<std::vector<std::vector<bool>>, std::map<std::pair<int, int>, std::vector<py::object>>>
    rebuildPassabilityCache(const sf::Vector2u &size);

    // BIND_PROPERTY
    py::object tilemapRef;

    // BIND_PROPERTY
    std::vector<std::string> layerKeysRef;

    // BIND_PROPERTY
    ActorDict actorsRef;

    // BIND_PROPERTY
    std::unordered_map<std::string, TileGrids> tileDataRef;

    // BIND_PROPERTY
    py::function getLayer;

    // BIND_PROPERTY
    py::function getMapPosition;

    // BIND_PROPERTY
    py::function getCollisionEnabled;

    // BIND_PROPERTY
    py::function TileLayerPassable;

private:
    sf::Vector3f castFromColor(const sf::Color &color);
    std::string getUniformArrayName(const std::string &name, int count);
    bool passable(int x, int y, int sx, int sy, int gx, int gy);
    py::object getMaterialProperty(const sf::Vector2i pos, const std::string &functionName,
                                   const py::object &invalidValue);
    sf::Shader *materialShader_;
    std::map<std::pair<std::string, int>, std::string> uniformArrayNameCache_;
};
