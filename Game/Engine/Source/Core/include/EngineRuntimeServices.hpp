#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

struct lua_State;
class Actor;

LUDORK_ENGINE_API void dispatchActorTick(Actor& actor, float deltaTime);
LUDORK_ENGINE_API void dispatchActorLateTick(Actor& actor, float deltaTime);
LUDORK_ENGINE_API void dispatchActorFixedTick(Actor& actor, float fixedDelta);
LUDORK_ENGINE_API void dispatchActorCollision(
    Actor& actor, const std::vector<Actor*>& others);
LUDORK_ENGINE_API void dispatchActorOverlap(Actor& actor,
                                            const std::vector<Actor*>& others);

BIND_MODULE_INIT()
LUDORK_ENGINE_API void initializeEngineRuntimeServices(lua_State* state);

LUDORK_ENGINE_API void shutdownEngineRuntimeServices(lua_State* state) noexcept;

LUDORK_ENGINE_API void initializeLatent();
