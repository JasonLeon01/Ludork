#pragma once
#include <BindAnnotations.hpp>
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

////////////////////////////////////////////////////////////
/// \brief Accelerated game map helper for lighting and navigation
///
////////////////////////////////////////////////////////////
BIND_CLASS()
class GameMapExt {
public:
    GameMapExt() = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Construct game map helper with a shader target
    ///
    /// - \param shader Material shader used by `refreshShader`
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    GameMapExt(sf::Shader *shader);

    ////////////////////////////////////////////////////////////
    /// \brief Refresh material shader uniforms with map/light state
    ///
    /// - \param lightMask Light mask texture source
    /// - \param screenScale Screen scale factor
    /// - \param screenSize Screen size in pixels
    /// - \param viewPos Camera view position
    /// - \param viewRot Camera view rotation
    /// - \param gridSize Grid size in world units
    /// - \param cellSize Grid cell size
    /// - \param lights Active light objects
    /// - \param ambientColor Global ambient light color
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void refreshShader(const sf::RenderTexture &lightMask, float screenScale, const sf::Vector2f &screenSize,
                       const sf::Vector2f &viewPos, float viewRot, const sf::Vector2f &gridSize, int cellSize,
                       const std::vector<py::object> &lights, const sf::Color &ambientColor);

    ////////////////////////////////////////////////////////////
    /// \brief Build a grayscale texture from a material map
    ///
    /// - \param size Output texture size
    /// - \param materialMap Material values in range `[0, 1]`
    /// - \param smooth Whether smoothing should be enabled
    ///
    /// - \return Newly allocated texture pointer
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    sf::Texture *generateDataFromMap(const sf::Vector2u &size, const std::vector<std::vector<float>> &materialMap,
                                     bool smooth = false);

    ////////////////////////////////////////////////////////////
    /// \brief Find path moves between two grid positions using A*
    ///
    /// - \param start Start position
    /// - \param goal Goal position
    /// - \param size Grid dimensions
    ///
    /// - \return Sequence of per-step movement vectors
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<sf::Vector2i> findPathExt(const sf::Vector2i &start, const sf::Vector2i &goal,
                                          const sf::Vector2u &size);

    ////////////////////////////////////////////////////////////
    /// \brief Build a 2D map of dynamic material property values
    ///
    /// - \param width Grid width
    /// - \param height Grid height
    /// - \param functionName Property function name to evaluate
    /// - \param invalidValue Fallback value for invalid positions
    ///
    /// - \return 2D property map
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<std::vector<py::object>> getMaterialPropertyMapExt(int width, int height,
                                                                   const std::string &functionName,
                                                                   const py::object &invalidValue);

    ////////////////////////////////////////////////////////////
    /// \brief Recompute tile passability and actor occupancy caches
    ///
    /// - \param size Grid dimensions
    ///
    /// - \return Pair of passability grid and occupancy map
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::pair<std::vector<std::vector<bool>>, std::map<std::pair<int, int>, std::vector<py::object>>>
    rebuildPassabilityCache(const sf::Vector2u &size);

    ////////////////////////////////////////////////////////////
    /// \brief Python tilemap object reference
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    py::object tilemapRef;

    ////////////////////////////////////////////////////////////
    /// \brief Ordered layer key list
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    std::vector<std::string> layerKeysRef;

    ////////////////////////////////////////////////////////////
    /// \brief Layer to actor list mapping
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    ActorDict actorsRef;

    ////////////////////////////////////////////////////////////
    /// \brief Layered tile data references
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    std::unordered_map<std::string, TileGrids> tileDataRef;

    ////////////////////////////////////////////////////////////
    /// \brief Python callback for retrieving a layer object
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    py::function getLayer;

    ////////////////////////////////////////////////////////////
    /// \brief Python callback for querying actor map position
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    py::function getMapPosition;

    ////////////////////////////////////////////////////////////
    /// \brief Python callback for actor collision enable flag
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    py::function getCollisionEnabled;

    ////////////////////////////////////////////////////////////
    /// \brief Python callback for tile passability lookup
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    py::function TileLayerPassable;

private:
    ////////////////////////////////////////////////////////////
    /// \brief Convert `sf::Color` into normalized shader vector
    ///
    /// - \param color Source color
    ///
    /// - \return Normalized RGB vector
    ///
    ////////////////////////////////////////////////////////////
    sf::Vector3f castFromColor(const sf::Color &color);

    ////////////////////////////////////////////////////////////
    /// \brief Build and cache GLSL uniform array element names
    ///
    /// - \param name Uniform base name
    /// - \param count Uniform index
    ///
    /// - \return Cached uniform field name
    ///
    ////////////////////////////////////////////////////////////
    std::string getUniformArrayName(const std::string &name, int count);

    ////////////////////////////////////////////////////////////
    /// \brief Check whether one grid node is traversable
    ///
    /// - \param x Node X coordinate
    /// - \param y Node Y coordinate
    /// - \param sx Start node X coordinate
    /// - \param sy Start node Y coordinate
    /// - \param gx Goal node X coordinate
    /// - \param gy Goal node Y coordinate
    ///
    /// - \return `true` when traversable
    ///
    ////////////////////////////////////////////////////////////
    bool passable(int x, int y, int sx, int sy, int gx, int gy);

    ////////////////////////////////////////////////////////////
    /// \brief Query dynamic material property at one map position
    ///
    /// - \param pos Tile position
    /// - \param functionName Property function name
    /// - \param invalidValue Fallback value
    ///
    /// - \return Property value or `invalidValue`
    ///
    ////////////////////////////////////////////////////////////
    py::object getMaterialProperty(const sf::Vector2i pos, const std::string &functionName,
                                   const py::object &invalidValue);
    sf::Shader *materialShader_;
    std::map<std::pair<std::string, int>, std::string> uniformArrayNameCache_;
};
