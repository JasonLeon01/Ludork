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

class GameMapExt {
public:
    GameMapExt() = delete;
    GameMapExt(sf::Shader *shader);
    void refreshShader(const sf::RenderTexture &lightMask, float screenScale, const sf::Vector2f &screenSize,
                       const sf::Vector2f &viewPos, float viewRot, const sf::Vector2f &gridSize, int cellSize,
                       const std::vector<py::object> &lights, const sf::Color &ambientColor);
    sf::Texture *generateDataFromMap(const sf::Vector2u &size, const std::vector<std::vector<float>> &materialMap,
                                     bool smooth = false);
    std::vector<sf::Vector2i> findPathExt(const sf::Vector2i &start, const sf::Vector2i &goal,
                                          const sf::Vector2u &size);
    std::vector<std::vector<py::object>> getMaterialPropertyMapExt(int width, int height,
                                                                   const std::string &functionName,
                                                                   const py::object &invalidValue);
    std::pair<std::vector<std::vector<bool>>, std::map<std::pair<int, int>, std::vector<py::object>>>
    rebuildPassabilityCache(const sf::Vector2u &size);
    py::object tilemapRef;
    std::vector<std::string> layerKeysRef;
    ActorDict actorsRef;
    std::unordered_map<std::string, TileGrids> tileDataRef;
    py::function getLayer;
    py::function getMapPosition;
    py::function getCollisionEnabled;
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
