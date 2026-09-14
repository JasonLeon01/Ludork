#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <General/Material.hpp>

class Actor;

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API ActorMapService : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(ActorMapService, RuntimeObject)

    ~ActorMapService() override;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2u getSize() const = 0;

    BIND_METHOD(Pure = true)
    virtual bool isPassable(const Actor& actor,
                            const sf::Vector2i& position) const = 0;

    BIND_METHOD()
    virtual std::vector<Actor*> getCollision(Actor& actor,
                                             const sf::Vector2i& position) = 0;

    BIND_METHOD()
    virtual std::vector<Actor*> getOverlaps(Actor& actor) = 0;

    BIND_METHOD(Pure = true)
    virtual std::optional<Material> getTopMaterial(
        const sf::Vector2i& position) const = 0;

    BIND_METHOD()
    virtual void updateActorOccupancy(Actor& actor) = 0;

    BIND_METHOD()
    virtual void updateActorList() = 0;

    BIND_METHOD()
    virtual void destroyActor(Actor& actor) = 0;
};
