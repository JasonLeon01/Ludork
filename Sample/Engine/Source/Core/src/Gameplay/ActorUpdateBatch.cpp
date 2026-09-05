#include <Gameplay/Actor.hpp>

#include <Runtime/Blueprint/BPBase.hpp>

namespace {

constexpr unsigned int actorTickEvent = 1U;
constexpr unsigned int actorLateTickEvent = 2U;
constexpr unsigned int actorFixedTickEvent = 4U;

}  // namespace

ActorUpdateBatch::ActorUpdateBatch() = default;

void ActorUpdateBatch::syncActors(
    const std::vector<std::shared_ptr<Actor>>& actors) {
    actors_ = actors;
    tickEvents_.clear();
    for (const std::shared_ptr<Actor>& actor : actors_) {
        if (!actor) {
            continue;
        }
        unsigned int events = 0U;
        if (BPBase::HasBlueprintEventNative(*actor, "onTick")) {
            events |= actorTickEvent;
        }
        if (BPBase::HasBlueprintEventNative(*actor, "onLateTick")) {
            events |= actorLateTickEvent;
        }
        if (BPBase::HasBlueprintEventNative(*actor, "onFixedTick")) {
            events |= actorFixedTickEvent;
        }
        tickEvents_.emplace(actor.get(), events);
        if (actor->getTickable() && events == 0U) {
            actor->setTickable(false, false);
        }
    }
}

void ActorUpdateBatch::update(float deltaTime) {
    for (const std::shared_ptr<Actor>& actor : actors_) {
        if (!actor) {
            continue;
        }
        actor->update(deltaTime);
        const auto events = tickEvents_.find(actor.get());
        if (actor->getTickable() && events != tickEvents_.end() &&
            (events->second & actorTickEvent) != 0U) {
            BPBase::BlueprintEventNative(
                *actor, "onTick", {{"deltaTime", RuntimeValue(deltaTime)}});
        }
    }
}

void ActorUpdateBatch::lateUpdate(float deltaTime) {
    for (const std::shared_ptr<Actor>& actor : actors_) {
        if (!actor) {
            continue;
        }
        actor->lateUpdate(deltaTime);
        const auto events = tickEvents_.find(actor.get());
        if (actor->getTickable() && events != tickEvents_.end() &&
            (events->second & actorLateTickEvent) != 0U) {
            BPBase::BlueprintEventNative(
                *actor, "onLateTick", {{"deltaTime", RuntimeValue(deltaTime)}});
        }
    }
}

void ActorUpdateBatch::fixedUpdate(float fixedDelta) {
    for (const std::shared_ptr<Actor>& actor : actors_) {
        if (!actor) {
            continue;
        }
        actor->fixedUpdate(fixedDelta);
        const auto events = tickEvents_.find(actor.get());
        if (actor->getTickable() && events != tickEvents_.end() &&
            (events->second & actorFixedTickEvent) != 0U) {
            BPBase::BlueprintEventNative(
                *actor, "onFixedTick",
                {{"fixedDelta", RuntimeValue(fixedDelta)}});
        }
    }
}
