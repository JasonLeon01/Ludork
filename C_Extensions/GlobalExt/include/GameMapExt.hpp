#pragma once
#include <BindAnnotations.hpp>
#include <Light.hpp>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace py = pybind11;
using ActorDict = std::unordered_map<std::string, std::vector<py::object>>;
using TileGrids = std::vector<std::vector<std::optional<int>>>;
using IntPair = std::pair<int, int>;
struct IntPairHash {
    std::size_t operator()(const IntPair &value) const;
};
using OccupancyMap = std::unordered_map<IntPair, std::vector<py::object>, IntPairHash>;
using LayerVisibleMap = std::unordered_map<std::string, bool>;
using LayerPassableMap = std::unordered_map<std::string, std::vector<bool>>;

////////////////////////////////////////////////////////////
/// \brief Pathfinding result in all runtime path formats
///
////////////////////////////////////////////////////////////
BIND_CLASS(copyable = true)
struct PathResult {
    ////////////////////////////////////////////////////////////
    /// \brief Per-step movement offsets
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    std::vector<sf::Vector2i> offsets;

    ////////////////////////////////////////////////////////////
    /// \brief Absolute path points excluding the start position
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    std::vector<sf::Vector2i> points;

    ////////////////////////////////////////////////////////////
    /// \brief Absolute route including the start position
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    std::vector<sf::Vector2i> route;
};

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
                       const std::vector<Light> &lights, const sf::Color &ambientColor);

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
    /// \brief Find path data between two grid positions using A*
    ///
    /// - \param start Start position
    /// - \param goal Goal position
    /// - \param size Grid dimensions
    ///
    /// - \return Pathfinding result containing offsets, points, and route
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    PathResult findPathExt(const sf::Vector2i &start, const sf::Vector2i &goal, const sf::Vector2u &size);

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
    /// - \return Tile passability grid
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<std::vector<bool>> rebuildPassabilityCache(const sf::Vector2u &size);

    ////////////////////////////////////////////////////////////
    /// \brief Get actors cached at one map position
    ///
    /// - \param x Tile X coordinate
    /// - \param y Tile Y coordinate
    ///
    /// - \return Actors at the requested position
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<py::object> getActorsAt(int x, int y);

    ////////////////////////////////////////////////////////////
    /// \brief Get actors cached inside a square tile range
    ///
    /// - \param x Centre tile X coordinate
    /// - \param y Centre tile Y coordinate
    /// - \param radius Range radius in tiles
    ///
    /// - \return Actors inside the requested range
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<py::object> getActorsInRange(int x, int y, int radius);

    ////////////////////////////////////////////////////////////
    /// \brief Get colliding actors cached at one map position
    ///
    /// - \param x Tile X coordinate
    /// - \param y Tile Y coordinate
    /// - \param selfActor Actor excluded from the result
    ///
    /// - \return Colliding actors at the requested position
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<py::object> getCollisionAt(int x, int y, const py::object &selfActor);

    ////////////////////////////////////////////////////////////
    /// \brief Get overlapping actors cached at one map position
    ///
    /// - \param x Tile X coordinate
    /// - \param y Tile Y coordinate
    /// - \param selfActor Actor excluded from the result
    ///
    /// - \return Overlapping actors at the requested position
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<py::object> getOverlapsAt(int x, int y, const py::object &selfActor);

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
    /// \brief Layered autotile data references
    ///
    /// Each cell stores the autotile pool index (or `std::nullopt`
    /// when the cell has no autotile). Layers without autotiles may
    /// omit their entry from the map.
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    std::unordered_map<std::string, TileGrids> autoTileDataRef;

    ////////////////////////////////////////////////////////////
    /// \brief Per-layer visibility state
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    LayerVisibleMap layerVisibleRef;

    ////////////////////////////////////////////////////////////
    /// \brief Per-layer static tile passability table
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    LayerPassableMap tilePassableRef;

    ////////////////////////////////////////////////////////////
    /// \brief Per-layer autotile passability table
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    LayerPassableMap autoTilePassableRef;

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
    /// \brief Convert ambient `sf::Color` into normalized shader vector
    ///
    /// - \param color Source color
    ///
    /// - \return Normalized RGB vector scaled by alpha
    ///
    ////////////////////////////////////////////////////////////
    sf::Vector3f castAmbientFromColor(const sf::Color &color);

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
    /// \brief Check whether a layer should be considered for tile passability
    ///
    /// - \param layerName Layer key
    /// - \return True if the layer is visible
    ///
    ////////////////////////////////////////////////////////////
    bool layerVisible(const std::string &layerName) const;

    ////////////////////////////////////////////////////////////
    /// \brief Resolve one layer cell passability from raw tile/autotile tables
    ///
    /// - \param layerName Layer key
    /// - \param x Tile X coordinate
    /// - \param y Tile Y coordinate
    /// - \param outPassable Passability result when content exists
    /// - \return True when the layer has tile or autotile content at the cell
    ///
    ////////////////////////////////////////////////////////////
    bool tryGetLayerPassability(const std::string &layerName, int x, int y, bool &outPassable) const;

    ////////////////////////////////////////////////////////////
    /// \brief Check whether an actor object has been destroyed
    ///
    /// - \param actor Actor object to query
    ///
    /// - \return `true` when the actor reports destroyed
    ///
    ////////////////////////////////////////////////////////////
    bool actorDestroyed(const py::object &actor) const;

    ////////////////////////////////////////////////////////////
    /// \brief Collect descendant actor identities for filtering child actors
    ///
    /// - \param actor Root actor object
    ///
    /// - \return Set of descendant Python object pointers
    ///
    ////////////////////////////////////////////////////////////
    std::unordered_set<PyObject *> getDescendantActorPointers(const py::object &actor) const;

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
    OccupancyMap occupancyMap_;
    std::map<std::pair<std::string, int>, std::string> uniformArrayNameCache_;
};
