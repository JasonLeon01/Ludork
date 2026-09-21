#include <Gameplay/Actor.hpp>
#include <EngineRuntimeServices.hpp>

#include <Runtime/Blueprint/BlueprintRuntime.hpp>

namespace {

constexpr unsigned int actorTickEvent = 1U;
constexpr unsigned int actorLateTickEvent = 2U;
constexpr unsigned int actorFixedTickEvent = 4U;
const std::vector<std::string> actorEventNames{"onTick", "onLateTick",
                                               "onFixedTick"};

RuntimeValue actorRuntimeValue(const std::shared_ptr<Actor>& actor) {
    const std::shared_ptr<RuntimeObject> owner = actor->runtimeOwner();
    return RuntimeValue(owner ? owner : actor);
}

unsigned int cacheTickEvents(
    std::unordered_map<Actor*, unsigned int>& tickEvents, Actor& actor,
    const std::vector<bool>& events) {
    unsigned int mask = 0U;
    if (events[0]) {
        mask |= actorTickEvent;
    }
    if (events[1]) {
        mask |= actorLateTickEvent;
    }
    if (events[2]) {
        mask |= actorFixedTickEvent;
    }
    tickEvents.emplace(&actor, mask);
    return mask;
}

unsigned int getTickEvents(std::unordered_map<Actor*, unsigned int>& tickEvents,
                           const std::shared_ptr<Actor>& actor) {
    const auto cached = tickEvents.find(actor.get());
    if (cached != tickEvents.end()) {
        return cached->second;
    }
    const std::vector<std::vector<bool>> events = blueprintRuntime().hasEvents(
        {actorRuntimeValue(actor)}, actorEventNames);
    return cacheTickEvents(tickEvents, *actor, events.front());
}

}  // namespace

ActorUpdateBatch::ActorUpdateBatch() = default;

void ActorUpdateBatch::syncActors(
    const std::vector<std::shared_ptr<Actor>>& actors) {
    actors_ = actors;
    tickEvents_.clear();
    if (actors_.empty()) {
        return;
    }
    std::vector<RuntimeValue> objects;
    std::vector<Actor*> tickableActors;
    objects.reserve(actors_.size());
    tickableActors.reserve(actors_.size());
    for (const std::shared_ptr<Actor>& actor : actors_) {
        if (actor && actor->getTickable()) {
            objects.push_back(actorRuntimeValue(actor));
            tickableActors.push_back(actor.get());
        }
    }
    const std::vector<std::vector<bool>> actorEvents =
        blueprintRuntime().hasEvents(objects, actorEventNames);
    for (std::size_t index = 0; index < tickableActors.size(); ++index) {
        Actor& actor = *tickableActors[index];
        const unsigned int events =
            cacheTickEvents(tickEvents_, actor, actorEvents[index]);
        if (actor.getTickable() && events == 0U) {
            actor.setTickable(false, false);
        }
    }
}

void ActorUpdateBatch::update(float deltaTime) {
    for (const std::shared_ptr<Actor>& actor : actors_) {
        if (!actor) {
            continue;
        }
        actor->update(deltaTime);
        if (actor->getTickable() &&
            (getTickEvents(tickEvents_, actor) & actorTickEvent) != 0U) {
            dispatchActorTick(*actor, deltaTime);
        }
    }
}

void ActorUpdateBatch::lateUpdate(float deltaTime) {
    for (const std::shared_ptr<Actor>& actor : actors_) {
        if (!actor) {
            continue;
        }
        actor->lateUpdate(deltaTime);
        if (actor->getTickable() &&
            (getTickEvents(tickEvents_, actor) & actorLateTickEvent) != 0U) {
            dispatchActorLateTick(*actor, deltaTime);
        }
    }
}

void ActorUpdateBatch::fixedUpdate(float fixedDelta) {
    for (const std::shared_ptr<Actor>& actor : actors_) {
        if (!actor) {
            continue;
        }
        actor->fixedUpdate(fixedDelta);
        if (actor->getTickable() &&
            (getTickEvents(tickEvents_, actor) & actorFixedTickEvent) != 0U) {
            dispatchActorFixedTick(*actor, fixedDelta);
        }
    }
}
