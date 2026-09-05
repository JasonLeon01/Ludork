#include <EngineRuntimeServices.hpp>

#include <Input/InputService.hpp>
#include <Runtime/Blueprint/BlueprintRuntime.hpp>
#include <Runtime/NodeGraph/LatentManager.hpp>
#include <Runtime/RuntimeReference.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <RuntimeSession.hpp>
#include <Utils/EventBus.hpp>

#include <stdexcept>

namespace {

RuntimeValue actorGraph(const RuntimeValue& object) {
    using namespace ludork::runtime::reference;
    const RuntimeValue engine = rawGet(globals(), "Engine");
    if (!isTable(engine)) {
        throw std::runtime_error("Engine module is not initialized");
    }
    const RuntimeValue actorType = rawGet(intern(engine), "Actor");
    if (!isTable(actorType) || !isInstance(object, actorType)) {
        return {};
    }
    const RuntimeValue method = get(intern(object), "getGraph");
    return isFunction(method) ? first(invoke(intern(method), {object}))
                              : RuntimeValue();
}

}  // namespace

void initializeEngineRuntimeServices(lua_State* state) {
    if (state == nullptr) {
        return;
    }
    ludork::runtime::initialize(state);
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
    EventBus::setBlueprintEventValidator({});
    EventBus::setBlueprintEventInvoker({});
    inputService().setFrameCompletionCallback({});
    if (state == nullptr) {
        return;
    }
    ludork::runtime::shutdown(state);
}
