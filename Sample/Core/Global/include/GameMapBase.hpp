#pragma once
#include <BindAnnotations.hpp>
#include <Gameplay/Actor.hpp>
#include <Gameplay/TileMap.hpp>
#include <General/Material.hpp>
#include <Light.hpp>

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <array>
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

class GameMapActorRegistry;

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

BIND_CLASS(copyable = true, table_init = true, metadata = false)
struct LightOcclusionInput {
    BIND_PROPERTY(metadata = false)
    Light light;

    BIND_PROPERTY(metadata = false)
    std::shared_ptr<Actor> owner;
};

BIND_CLASS(copyable = true, metadata = false)
struct LightOcclusionResult {
    BIND_PROPERTY(metadata = false)
    bool hasStaticTransmissionLoss = false;

    BIND_PROPERTY(metadata = false)
    std::vector<std::shared_ptr<Actor>> occluders;

    BIND_PROPERTY(metadata = false)
    std::optional<sf::FloatRect> maskRect;

    BIND_PROPERTY(metadata = false)
    std::shared_ptr<sf::Texture> dynamicOccupancy;

    BIND_PROPERTY(metadata = false)
    sf::Vector2f dynamicOccupancyOrigin;

    BIND_PROPERTY(metadata = false)
    sf::Vector2f dynamicOccupancySize;
};

////////////////////////////////////////////////////////////
/// \brief Native game map base for material queries and navigation
///
////////////////////////////////////////////////////////////
BIND_CLASS()
class GameMapBase : public ActorMapService {
public:
    BIND_INIT()
    GameMapBase();

    ~GameMapBase() override;

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

    BIND_METHOD(metadata = false)
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

    BIND_METHOD(metadata = false)
    void configureSparseWorld(const sf::Vector2u& size,
                              const std::vector<std::string>& layerOrder,
                              const std::vector<sf::IntRect>& regionRects);

    BIND_METHOD(metadata = false)
    void setSparseWorldRegion(int regionIndex, std::shared_ptr<Tilemap> tilemap,
                              bool actorsReady);

    BIND_METHOD(metadata = false)
    void setSparseWorldRegionActorsReady(int regionIndex);

    BIND_METHOD(metadata = false)
    void detachSparseWorldRegion(int regionIndex);

    BIND_METHOD(metadata = false)
    void setSparseWorldPreparedRect(std::optional<sf::IntRect> rect);

    BIND_METHOD(metadata = false)
    bool isSparseWorldCellReady(const sf::Vector2i& position) const;

    BIND_METHOD(metadata = false)
    bool isSparseWorldGameplayPositionReady(const sf::Vector2i& position) const;

    BIND_METHOD(metadata = false)
    void clearSparseWorld();

    BIND_METHOD(metadata = false)
    std::size_t getSparseOccupancyPageCount() const;

    BIND_METHOD(metadata = false)
    std::shared_ptr<sf::Texture> rebuildStaticLightOccupancy(
        const sf::Vector2i& origin, const sf::Vector2u& size,
        const std::vector<std::shared_ptr<Actor>>& actors);

    BIND_METHOD(metadata = false)
    std::vector<LightOcclusionResult> analyseLightOcclusion(
        const std::vector<LightOcclusionInput>& inputs,
        const std::vector<std::shared_ptr<Actor>>& visibleActors);

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

    BIND_METHOD(name = "_registerLayerActor", metadata = false)
    bool registerLayerActor(ActorPtr actor, const std::string& layer);

    BIND_METHOD(name = "_getRegisteredActorLayer", metadata = false)
    std::optional<std::string> getRegisteredActorLayer(Actor& actor) const;

    BIND_METHOD(metadata = false)
    void beginActorBatch();

    BIND_METHOD(metadata = false)
    void endActorBatch();

    BIND_METHOD(name = "_flushActorChanges", metadata = false)
    void flushActorChanges();

    BIND_METHOD(name = "_withDeferredActorViewSync", metadata = false)
    void withDeferredActorViewSync(std::function<void()> handler);

    BIND_METHOD(name = "_drainActorLifecycle", metadata = false)
    void drainActorLifecycle(std::function<void(Actor&)> createHandler,
                             std::function<void(Actor&)> componentHandler);

    BIND_METHOD(name = "_syncActorViews", metadata = false)
    void syncActorViews(const ActorDict& actors);

    BIND_METHOD(name = "_forgetActors", metadata = false)
    void forgetActors(const std::vector<ActorPtr>& actors);

    BIND_METHOD(name = "_updateActors", metadata = false)
    void updateActors(float deltaTime);

    BIND_METHOD(name = "_lateUpdateActors", metadata = false)
    void lateUpdateActors(float deltaTime);

    BIND_METHOD(name = "_fixedUpdateActors", metadata = false)
    void fixedUpdateActors(float fixedDelta);

    const ActorDict& getMaterialActorsForRenderer() const;

    const ActorPtr& getPlayerActorForRenderer() const;

private:
    static constexpr int OccupancyPageSize = 32;

    struct SparseOccupancyPage {
        std::array<std::vector<Actor*>, OccupancyPageSize * OccupancyPageSize>
            cells;
        std::size_t occupiedCellCount = 0;
    };

    using SparseOccupancyPageMap =
        std::unordered_map<IntPair, SparseOccupancyPage, IntPairHash>;

    struct SparseWorldRegion {
        sf::IntRect rect;
        std::shared_ptr<Tilemap> tilemap;
        std::vector<std::shared_ptr<TileLayer>> layersTopFirst;
        bool actorsReady = false;
    };

    using SparseWorldRegionPageMap =
        std::unordered_map<IntPair, std::vector<std::size_t>, IntPairHash>;

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

    void clearActorOccupancy();

    const std::vector<Actor*>* findActorsAtCell(int x, int y) const;

    static int getOccupancyPageCoordinate(int value);
    static int getOccupancyPageOffset(int value);
    static IntPair getOccupancyPageKey(int x, int y);
    static std::size_t getOccupancyPageCellIndex(int x, int y);

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

    const SparseWorldRegion* findSparseWorldRegion(
        const sf::Vector2i& position) const;
    SparseWorldRegion& requireSparseWorldRegion(int regionIndex);
    bool isSparseWorldTilePassable(const sf::Vector2i& position) const;
    bool isSparseWorldDirectionPassable(const sf::Vector2i& fromPosition,
                                        const sf::Vector2i& toPosition,
                                        int direction) const;
    std::optional<Material> getSparseWorldTopMaterial(
        const sf::Vector2i& position) const;

    void clearStaticLightOccupancy();
    bool hasStaticLightOccupancy(const Light& light) const;

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
    std::optional<sf::Vector2u> sparseWorldSize_;
    std::vector<std::string> sparseWorldLayerOrder_;
    std::vector<SparseWorldRegion> sparseWorldRegions_;
    SparseWorldRegionPageMap sparseWorldRegionPages_;
    std::optional<sf::IntRect> sparseWorldPreparedRect_;
    std::shared_ptr<sf::Texture> staticLightOccupancy_;
    sf::Vector2i staticLightOccupancyOrigin_;
    sf::Vector2u staticLightOccupancySize_;
    std::vector<std::size_t> staticLightOccupancyPrefix_;
    std::vector<std::shared_ptr<sf::Texture>> dynamicLightOccupancies_;
    std::vector<std::vector<bool>> tilePassableGrid_;
    bool passabilityDirty_ = true;
    OccupancyMap occupancyMap_;
    SparseOccupancyPageMap sparseOccupancyPages_;
    std::unordered_map<Actor*, std::vector<sf::Vector2i>>
        registeredOccupancyCells_;
    ActorDict actorsRef_;
    ActorDict materialActorsRef_;
    std::unordered_map<Actor*, std::string> actorLayerRef_;
    ActorPtr playerActor_;
    std::function<void()> actorListUpdater_;
    std::function<void(Actor&)> actorDestroyer_;
    std::unique_ptr<GameMapActorRegistry> actorRegistry_;
};
