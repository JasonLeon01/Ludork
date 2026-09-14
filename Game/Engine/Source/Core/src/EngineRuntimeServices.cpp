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

RuntimeValue actorGraph(const RuntimeValue& object) {
    const std::shared_ptr<Actor> actor =
        ludork::Cast<Actor>(ludork::runtime::reference::object(object));
    const std::shared_ptr<Graph> graph =
        actor == nullptr ? nullptr : actor->getGraph();
    return graph == nullptr ? RuntimeValue() : RuntimeValue(graph);
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
