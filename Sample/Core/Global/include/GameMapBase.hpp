#pragma once
#include <BindAnnotations.hpp>
#include <Gameplay/Actor.hpp>
#include <Gameplay/TileMap.hpp>
#include <General/Material.hpp>

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using ActorPtr = std::shared_ptr<Actor>;
using ActorDict = std::unordered_map<std::string, std::vector<ActorPtr>>;
using IntPair = std::pair<int, int>;
struct IntPairHash {
    std::size_t operator()(const IntPair& value) const;
};
using OccupancyMap =
    std::unordered_map<IntPair, std::vector<Actor*>, IntPairHash>;

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
/// \brief Native game map base for material queries and navigation
///
////////////////////////////////////////////////////////////
BIND_CLASS()
class GameMapBase : public ActorMapService {
public:
    BIND_INIT()
    GameMapBase() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Build a grayscale texture from a material map
    ///
    /// - \param size Output texture size
    /// - \param materialMap Material values in range `[0, 1]`
    /// - \param smooth Whether smoothing should be enabled
    ///
    /// - \return Shared texture resource
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::shared_ptr<sf::Texture> generateDataFromMap(
        const sf::Vector2u& size,
        const std::vector<std::vector<MaterialValue>>& materialMap,
        bool smooth = false);

    ////////////////////////////////////////////////////////////
    /// \brief Find path data between two grid positions using A*
    ///
    /// - \param start Start position
    /// - \param goal Goal position
    /// - \param size Grid dimensions
    /// - \param movingActor Actor whose footprint and occupancy are evaluated
    /// - \param excludedAnchors Anchor cells that A* must not enter
    ///
    /// - \return Pathfinding result containing offsets, points, and route
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(defaults = {nil, nil, nil, nil, {}})
    PathResult findPathExt(
        const sf::Vector2i& start, const sf::Vector2i& goal,
        const sf::Vector2u& size, Actor& movingActor,
        const std::vector<sf::Vector2i>& excludedAnchors = {});

    ////////////////////////////////////////////////////////////
    /// \brief Build a 2D map of dynamic material property values
    ///
    /// - \param width Grid width
    /// - \param height Grid height
    /// - \param propertyName Material property name to evaluate
    /// - \param invalidValue Fallback value for invalid positions
    ///
    /// - \return 2D property map
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<std::vector<MaterialValue>> getMaterialPropertyMapExt(
        int width, int height, const std::string& propertyName,
        const MaterialValue& invalidValue);

    ////////////////////////////////////////////////////////////
    /// \brief Recompute tile passability and actor occupancy caches
    ///
    /// - \param size Grid dimensions
    ///
    /// - \return Tile passability grid
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<std::vector<bool>> rebuildPassabilityCache(
        const sf::Vector2u& size);

    BIND_METHOD(metadata = false)
    void invalidatePassabilityCache();

    ////////////////////////////////////////////////////////////
    /// \brief Refresh one actor's occupancy without rebuilding tile passability
    ///
    /// Removes the actor from all cached cells, then registers it at its
    /// current occupied cells. Use this after an actor changes map position.
    ///
    /// - \param actor Actor whose occupancy should be refreshed
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void updateActorOccupancy(Actor& actor) override;

    sf::Vector2u getSize() const override;
    bool isPassable(const Actor& actor,
                    const sf::Vector2i& position) const override;
    std::vector<Actor*> getCollision(Actor& actor,
                                     const sf::Vector2i& position) override;
    std::vector<Actor*> getOverlaps(Actor& actor) override;
    std::optional<Material> getTopMaterial(
        const sf::Vector2i& position) const override;
    void updateActorList() override;
    void destroyActor(Actor& actor) override;

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
    std::vector<Actor*> getActorsAt(int x, int y);

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
    std::vector<Actor*> getActorsInRange(int x, int y, int radius);

    BIND_METHOD(metadata = false)
    std::vector<Actor*> getActorsInRangeExcluding(int x, int y, int radius,
                                                  Actor& excludedActor);

    ////////////////////////////////////////////////////////////
    /// \brief Get colliding actors cached at one map position
    ///
    /// - \param x Tile X coordinate
    /// - \param y Tile Y coordinate
    /// - \param selfActor Actor excluded from the result
    ///
    /// - \return Colliding actors on the topmost occupied layer, ordered from
    /// visually topmost to bottommost
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::vector<Actor*> getCollisionAt(int x, int y, Actor& selfActor);

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
    std::vector<Actor*> getOverlapsAt(int x, int y, Actor& selfActor);

    ////////////////////////////////////////////////////////////
    /// \brief Set the native tilemap used by material and passability queries
    ///
    /// - \param tilemap Tilemap to retain
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void setTilemap(std::shared_ptr<Tilemap> tilemap);

    ////////////////////////////////////////////////////////////
    /// \brief Synchronise cached actor pointers from typed actor lists
    ///
    /// - \param actors Layer name to actor list mapping
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void syncActorsRef(const ActorDict& actors);

    BIND_METHOD(metadata = false)
    void syncMaterialActorsRef(const ActorDict& actors);

    BIND_METHOD(metadata = false)
    void setActorListUpdater(std::function<void()> updater);

    BIND_METHOD(metadata = false)
    void setActorDestroyer(std::function<void(Actor&)> destroyer);

    BIND_METHOD(metadata = false)
    void setPlayerActor(ActorPtr actor = nullptr);

private:
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

    bool passableForActor(int x, int y, int sx, int sy, int gx, int gy,
                          const Actor* excludedActor);

    ////////////////////////////////////////////////////////////
    /// \brief Register one actor into all occupied cells in the occupancy map
    ///
    /// - \param actor Actor object to register
    ///
    ////////////////////////////////////////////////////////////
    void registerActorOccupancy(Actor& actor);

    ////////////////////////////////////////////////////////////
    /// \brief Remove one actor from every cell in the occupancy map
    ///
    /// - \param actor Actor object to unregister
    ///
    ////////////////////////////////////////////////////////////
    void unregisterActorOccupancy(Actor& actor);

    std::vector<Actor*> getActorsInRangeImpl(int x, int y, int radius,
                                             const Actor* excludedActor);

    ////////////////////////////////////////////////////////////
    /// \brief Check whether a pathfinding transition is passable for a moving
    /// actor
    ///
    /// - \param fromX Source anchor X coordinate
    /// - \param fromY Source anchor Y coordinate
    /// - \param x Node anchor X coordinate
    /// - \param y Node anchor Y coordinate
    /// - \param sx Start anchor X coordinate
    /// - \param sy Start anchor Y coordinate
    /// - \param gx Goal anchor X coordinate
    /// - \param gy Goal anchor Y coordinate
    /// - \param width Search grid width
    /// - \param height Search grid height
    /// - \param movingActor Moving actor used for footprint checks
    ///
    /// - \return `true` when the node and directional transition are passable
    ///
    ////////////////////////////////////////////////////////////
    bool transitionPassableForActor(int fromX, int fromY, int x, int y, int sx,
                                    int sy, int gx, int gy, unsigned int width,
                                    unsigned int height,
                                    const Actor& movingActor);

    bool directionPassableForActor(const sf::Vector2i& fromPosition,
                                   const sf::Vector2i& toPosition,
                                   const std::vector<sf::Vector2i>& toCells,
                                   const Actor& movingActor) const;

    bool isDirectionPassable(const sf::Vector2i& fromPosition,
                             const sf::Vector2i& toPosition,
                             int direction) const;

    void ensurePassabilityCache() const;
    void refreshActorOccupancyCache();

    ////////////////////////////////////////////////////////////
    /// \brief Return layer names in top-first order
    ///
    /// - \return Reversed tilemap layer name list
    ///
    ////////////////////////////////////////////////////////////
    std::vector<std::string> getTopFirstLayerNames() const;

    ////////////////////////////////////////////////////////////
    /// \brief Query dynamic material property at one map position
    ///
    /// - \param pos Tile position
    /// - \param propertyName Material property name
    /// - \param invalidValue Fallback value
    ///
    /// - \return Property value or `invalidValue`
    ///
    ////////////////////////////////////////////////////////////
    MaterialValue getMaterialProperty(const sf::Vector2i& pos,
                                      const std::string& propertyName,
                                      const MaterialValue& invalidValue) const;

    ////////////////////////////////////////////////////////////
    /// \brief Resolve an actor's layer index in top-first layer order
    ///
    /// - \param actor Actor to query
    ///
    /// - \return Layer index, or a large value when unknown
    ///
    ////////////////////////////////////////////////////////////
    int getActorLayerIndex(const Actor* actor) const;

    ////////////////////////////////////////////////////////////
    /// \brief Resolve the topmost occupant layer index at one cell
    ///
    /// - \param actorsAtCell Actors cached at the cell
    /// - \param selfActor Actor excluded from the lookup, or `nullptr` to
    /// consider all occupants
    ///
    /// - \return Topmost layer index, or a large value when empty
    ///
    ////////////////////////////////////////////////////////////
    int getTopmostOccupantLayerIndex(const std::vector<Actor*>& actorsAtCell,
                                     const Actor* selfActor) const;

    std::shared_ptr<Tilemap> tilemap_;
    std::vector<std::vector<bool>> tilePassableGrid_;
    bool passabilityDirty_ = true;
    OccupancyMap occupancyMap_;
    std::unordered_map<Actor*, std::vector<sf::Vector2i>>
        registeredOccupancyCells_;
    ActorDict actorsRef_;
    ActorDict materialActorsRef_;
    std::unordered_map<Actor*, std::string> actorLayerRef_;
    ActorPtr playerActor_;
    std::function<void()> actorListUpdater_;
    std::function<void(Actor&)> actorDestroyer_;
};
