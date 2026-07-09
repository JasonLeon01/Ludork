#pragma once

#include <BindAnnotations.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/System/Vector2.hpp>
#include <pybind11/pybind11.h>

#include <string>
#include <unordered_set>
#include <vector>

namespace py = pybind11;

////////////////////////////////////////////////////////////
/// \brief Core actor state shared for map navigation and collision
///
////////////////////////////////////////////////////////////
BIND_CLASS()
class ActorCore {
public:
    ////////////////////////////////////////////////////////////
    /// \brief Construct actor core state for one actor
    ///
    /// - \param cellSize Grid cell size in world units
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    explicit ActorCore(int cellSize = 32);

    ////////////////////////////////////////////////////////////
    /// \brief Synchronise cached bounds from sprite world state
    ///
    /// - \param position Actor world position
    /// - \param globalBounds Actor global axis-aligned bounds
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void syncFromGlobalBounds(const sf::Vector2f &position, const sf::FloatRect &globalBounds);

    ////////////////////////////////////////////////////////////
    /// \brief Synchronise cached bounds from sprite transform data
    ///
    /// - \param position Actor world position
    /// - \param scale Actor scale factors
    /// - \param origin Actor origin in local space
    /// - \param localBoundsSize Texture rectangle size in local space
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void syncFromSprite(const sf::Vector2f &position, const sf::Vector2f &scale, const sf::Vector2f &origin,
                        const sf::Vector2f &localBoundsSize);

    ////////////////////////////////////////////////////////////
    /// \brief Get the cached anchor map position
    ///
    /// - \return Anchor grid position
    ///
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(pos=Vector2i)
    BIND_METHOD()
    sf::Vector2i getMapPosition() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get cached global bounds
    ///
    /// - \return Global axis-aligned bounds
    ///
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(bounds=FloatRect)
    BIND_METHOD()
    sf::FloatRect getGlobalBounds() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get grid cells occupied at the current anchor position
    ///
    /// - \return Occupied grid cells
    ///
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(cells=List[Vector2i])
    BIND_METHOD()
    std::vector<sf::Vector2i> getOccupiedMapCells() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get grid cells occupied at a hypothetical anchor position
    ///
    /// - \param mapPosition Target anchor map position
    /// - \return Occupied grid cells at the target anchor
    ///
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(cells=List[Vector2i])
    BIND_METHOD()
    std::vector<sf::Vector2i> getOccupiedMapCellsAtMapPosition(const sf::Vector2i &mapPosition) const;

    BIND_PROPERTY()
    bool collisionEnabled = false;

    BIND_PROPERTY()
    bool destroyed = false;

    BIND_PROPERTY()
    std::string tag = "";

    ////////////////////////////////////////////////////////////
    /// \brief Get whether collision blocking is enabled
    ///
    /// - \return True when collision blocking is enabled
    ///
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(collisionEnabled=bool)
    BIND_METHOD()
    bool getCollisionEnabled() const { return collisionEnabled; }

    ////////////////////////////////////////////////////////////
    /// \brief Set whether collision blocking is enabled
    ///
    /// - \param enabled Collision blocking flag
    ///
    ////////////////////////////////////////////////////////////
    BIND_EXECSPLIT(default=None)
    BIND_METHOD()
    void setCollisionEnabled(bool enabled) { collisionEnabled = enabled; }

    ////////////////////////////////////////////////////////////
    /// \brief Get whether automatic pathfinding should treat this actor as blocking
    ///
    /// - \return True when pathfinding should avoid this actor
    ///
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(blocks=bool)
    BIND_METHOD()
    bool getPathfindingBlocks() const;

    ////////////////////////////////////////////////////////////
    /// \brief Set whether automatic pathfinding should treat this actor as blocking
    ///
    /// - \param blocks Pathfinding blocking flag
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void setPathfindingBlocks(bool blocks);

    ////////////////////////////////////////////////////////////
    /// \brief Get whether the actor has been destroyed
    ///
    /// - \return True when destroyed
    ///
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(destroyed=bool)
    BIND_METHOD()
    bool isDestroyed() const { return destroyed; }

    ////////////////////////////////////////////////////////////
    /// \brief Mark the actor destroyed state
    ///
    /// - \param destroyed Destroyed flag
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void markDestroyed(bool destroyed) { this->destroyed = destroyed; }

    ////////////////////////////////////////////////////////////
    /// \brief Attach a child actor to this hierarchy node
    ///
    /// - \param child Child actor to attach
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void addChild(ActorCore *child);

    ////////////////////////////////////////////////////////////
    /// \brief Detach a child actor from this hierarchy node
    ///
    /// - \param child Child actor to detach
    ///
    /// - \throw ValueError If the child is not found in the hierarchy
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void removeChild(ActorCore *child);

    ////////////////////////////////////////////////////////////
    /// \brief Set the parent actor without updating the parent's child list
    ///
    /// - \param parent Parent actor, or nullptr to clear
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void setParent(ActorCore *parent);

    ////////////////////////////////////////////////////////////
    /// \brief Get the parent actor
    ///
    /// - \return Parent actor, or nullptr when detached
    ///
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(parent=Optional[ActorCore])
    BIND_METHOD()
    ActorCore *getParent() const { return parent_; }

    ////////////////////////////////////////////////////////////
    /// \brief Get direct child actors
    ///
    /// - \return Child actors in hierarchy order
    ///
    ////////////////////////////////////////////////////////////
    BIND_RETURNTYPE(children=List[ActorCore])
    BIND_METHOD()
    const std::vector<ActorCore *> &getChildren() const { return children_; }

    ////////////////////////////////////////////////////////////
    /// \brief Refresh cached descendant actor cores from the child hierarchy
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void refreshDescendantIds();

    ////////////////////////////////////////////////////////////
    /// \brief Get the owning Python actor object
    ///
    /// - \return Python actor instance for this core
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    py::object getPythonActor() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get cached descendant actor cores
    ///
    /// - \return Descendant actor core pointers
    ///
    ////////////////////////////////////////////////////////////
    BIND_IGNORE()
    const std::unordered_set<ActorCore *> &getDescendantIds() const;

    ////////////////////////////////////////////////////////////
    /// \brief Check whether this actor blocks map passability checks
    ///
    /// - \return True when collision or pathfinding blocking is active
    ///
    ////////////////////////////////////////////////////////////
    BIND_IGNORE()
    bool blocksPassability() const;

private:
    std::vector<sf::Vector2i> computeOccupiedCells(const sf::FloatRect &bounds) const;

    int cellSize_;
    sf::Vector2f position_;
    sf::Vector2i mapPosition_;
    sf::FloatRect globalBounds_;
    std::vector<sf::Vector2i> occupiedCells_;
    bool pathfindingBlocks_;
    ActorCore *parent_ = nullptr;
    std::vector<ActorCore *> children_;
    std::unordered_set<ActorCore *> descendantIds_;
};
