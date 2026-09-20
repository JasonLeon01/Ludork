#include <EngineRuntimeServices.hpp>
#include <EngineLifecycle.hpp>

#include <Input/InputService.hpp>
#include <Gameplay/Actor.hpp>
#include <Runtime/Blueprint/BlueprintRuntime.hpp>
#include <Runtime/NodeGraph/LatentManager.hpp>
#include <Runtime/RuntimeReference.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <RuntimeSession.hpp>
#include <Utils/EventBus.hpp>

#include <stdexcept>

namespace {

std::shared_ptr<Graph> actorGraph(
    const std::shared_ptr<RuntimeObject>& object) {
    const std::shared_ptr<Actor> actor = ludork::Cast<Actor>(object);
    return actor == nullptr ? nullptr : actor->getGraph();
}

std::shared_ptr<RuntimeObject> actorOwner(Actor& actor) {
    std::shared_ptr<RuntimeObject> owner = actor.runtimeOwner();
    return owner != nullptr ? owner : actor.weak_from_this().lock();
}

void dispatchActorTimeEvent(Actor& actor, const char* eventName,
                            const char* parameterName, float value) {
    const std::shared_ptr<RuntimeObject> owner = actorOwner(actor);
    if (owner != nullptr) {
        blueprintRuntime().dispatchEventArguments(
            owner, eventName, {{parameterName, RuntimeValue(value)}});
    }
}

void dispatchActorContactEvent(Actor& actor, const char* eventName,
                               const std::vector<Actor*>& others) {
    const std::shared_ptr<RuntimeObject> owner = actorOwner(actor);
    if (owner == nullptr) {
        return;
    }
    RuntimeValue::Array values;
    values.reserve(others.size());
    for (Actor* other : others) {
        if (other != nullptr) {
            if (std::shared_ptr<RuntimeObject> otherOwner =
                    actorOwner(*other)) {
                values.emplace_back(std::move(otherOwner));
            }
        }
    }
    blueprintRuntime().dispatchEventArguments(
        owner, eventName, {{"other", RuntimeValue(std::move(values))}});
}

}  // namespace

void initializeEngineRuntimeServices(lua_State* state) {
    if (state == nullptr) {
        return;
    }
    ludork::standard::LuaExecutionScope execution(state);
    if (!execution.active()) {
        throw std::runtime_error(
            "Lua runtime session is unavailable or stopping");
    }
    state = execution.state();
    ludork::runtime::initialize(state);
    ludork::standard::registerRuntimeCleanup(state, ludork::engine::shutdown);
    blueprintRuntime().setObjectGraphResolver(actorGraph);
    EventBus::setBlueprintEventValidator(
        [state](const RuntimeIdentityPtr& object,
                const std::string& eventName) {
            ludork::standard::LuaExecutionScope execution(state);
            if (!execution.active()) {
                throw std::runtime_error("Lua runtime session is stopping");
            }
            blueprintRuntime().validateEvent(RuntimeValue(object), eventName);
        });
    EventBus::setBlueprintEventInvoker([state](const RuntimeIdentityPtr& object,
                                               const std::string& eventName) {
        ludork::standard::LuaExecutionScope execution(state);
        if (!execution.active()) {
            throw std::runtime_error("Lua runtime session is stopping");
        }
        const RuntimeValue target(object);
        blueprintRuntime().dispatchEvent(
            target,
            ludork::runtime::reference::identity(
                ludork::runtime::reference::classType(target)),
            eventName, ludork::runtime::reference::table(), {});
    });
}

void initializeLatent() {
    LatentManager& manager = latentManager();
    if (manager.isInitialised()) {
        return;
    }
    manager.setInitialised(true);
    inputService().setFrameCompletionCallback([] {
        latentManager().update();
    });
}

void shutdownEngineRuntimeServices(lua_State* state) noexcept {
    ludork::standard::LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    EventBus::setBlueprintEventValidator({});
    EventBus::setBlueprintEventInvoker({});
    inputService().setFrameCompletionCallback({});
}

void dispatchActorTick(Actor& actor, float deltaTime) {
    dispatchActorTimeEvent(actor, "onTick", "deltaTime", deltaTime);
}

void dispatchActorLateTick(Actor& actor, float deltaTime) {
    dispatchActorTimeEvent(actor, "onLateTick", "deltaTime", deltaTime);
}

void dispatchActorFixedTick(Actor& actor, float fixedDelta) {
    dispatchActorTimeEvent(actor, "onFixedTick", "fixedDelta", fixedDelta);
}

void dispatchActorCollision(Actor& actor, const std::vector<Actor*>& others) {
    dispatchActorContactEvent(actor, "onCollision", others);
}

void dispatchActorOverlap(Actor& actor, const std::vector<Actor*>& others) {
    dispatchActorContactEvent(actor, "onOverlap", others);
}
