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
class ActorCore;
using ActorDict = std::unordered_map<std::string, std::vector<py::object>>;
using ActorCoreDict = std::unordered_map<std::string, std::vector<ActorCore *>>;
using TileGrids = std::vector<std::vector<std::optional<int>>>;
using IntPair = std::pair<int, int>;
struct IntPairHash {
    std::size_t operator()(const IntPair &value) const;
};
using OccupancyMap = std::unordered_map<IntPair, std::vector<ActorCore *>, IntPairHash>;
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
    PathResult findPathExt(const sf::Vector2i &start, const sf::Vector2i &goal, const sf::Vector2u &size,
                           ActorCore &movingActor);

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
    std::vector<py::object> getCollisionAt(int x, int y, ActorCore &selfActor);

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
    std::vector<py::object> getOverlapsAt(int x, int y, ActorCore &selfActor);

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
    /// \brief Synchronise cached actor core pointers from Python actor lists
    ///
    /// - \param actors Layer name to Python actor list mapping
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void syncActorsRef(const ActorDict &actors);

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
    /// \brief Register one actor into all occupied cells in the occupancy map
    ///
    /// - \param actor Actor object to register
    ///
    ////////////////////////////////////////////////////////////
    void registerActorOccupancy(ActorCore &actor);

    ////////////////////////////////////////////////////////////
    /// \brief Check whether a pathfinding anchor node is passable for a moving actor
    ///
    /// - \param x Node anchor X coordinate
    /// - \param y Node anchor Y coordinate
    /// - \param sx Start anchor X coordinate
    /// - \param sy Start anchor Y coordinate
    /// - \param gx Goal anchor X coordinate
    /// - \param gy Goal anchor Y coordinate
    /// - \param movingActor Moving actor used for footprint checks
    ///
    /// - \return `true` when the node is passable
    ///
    ////////////////////////////////////////////////////////////
    bool nodePassableForActor(int x, int y, int sx, int sy, int gx, int gy, const ActorCore &movingActor);

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

    ////////////////////////////////////////////////////////////
    /// \brief Resolve an actor's layer index in top-first layer order
    ///
    /// - \param core Actor core to query
    ///
    /// - \return Layer index, or a large value when unknown
    ///
    ////////////////////////////////////////////////////////////
    int getActorLayerIndex(const ActorCore *core) const;

    ////////////////////////////////////////////////////////////
    /// \brief Resolve the topmost occupant layer index at one cell
    ///
    /// - \param actorsAtCell Actors cached at the cell
    /// - \param selfCore Actor excluded from the lookup, or `nullptr` to consider all occupants
    ///
    /// - \return Topmost layer index, or a large value when empty
    ///
    ////////////////////////////////////////////////////////////
    int getTopmostOccupantLayerIndex(const std::vector<ActorCore *> &actorsAtCell,
                                     const ActorCore *selfCore) const;

    sf::Shader *materialShader_;
    OccupancyMap occupancyMap_;
    ActorCoreDict actorsCoreRef_;
    std::unordered_map<ActorCore *, std::string> actorCoreLayerRef_;
    std::map<std::pair<std::string, int>, std::string> uniformArrayNameCache_;
};
