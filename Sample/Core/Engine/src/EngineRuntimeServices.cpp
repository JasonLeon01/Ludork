#include <EngineRuntimeServices.hpp>

#include <Gameplay/EngineClassRuntime.hpp>
#include <Gameplay/BlueprintRuntime/BlueprintRuntimeInternal.hpp>
#include <NodeGraph/Runtime/NodeGraphRuntimeInternal.hpp>
#include <Runtime/RuntimeReference.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <RuntimeSession.hpp>
#include <Utils/EventBus.hpp>

#include <stdexcept>

void initializeEngineRuntimeServices(lua_State* state) {
    if (state == nullptr) {
        return;
    }
    ludork::runtime::initialize(state);
    ludork::engine::runtime_detail::clearBlueprintRuntimeCaches();
    ludork::engine::runtime_detail::clearNodeGraphRuntimeCaches();
    EventBus::setBlueprintEventValidator(
        [state](const RuntimeIdentityPtr& object,
                const std::string& eventName) {
            ludork::standard::LuaExecutionScope execution(state);
            if (!execution.active()) {
                throw std::runtime_error("Lua runtime session is stopping");
            }
            ludork::engine::runtime_detail::validateBlueprintEvent(
                RuntimeValue(object), eventName);
        });
    EventBus::setBlueprintEventInvoker([state](const RuntimeIdentityPtr& object,
                                               const std::string& eventName) {
        ludork::standard::LuaExecutionScope execution(state);
        if (!execution.active()) {
            throw std::runtime_error("Lua runtime session is stopping");
        }
        const RuntimeValue target(object);
        ludork::engine::runtime_detail::dispatchBlueprintEvent(
            target, ludork::runtime::reference::classType(target), eventName,
            ludork::runtime::reference::table(), {});
    });
}

void shutdownEngineRuntimeServices(lua_State* state) noexcept {
    EventBus::setBlueprintEventValidator({});
    EventBus::setBlueprintEventInvoker({});
    if (state == nullptr) {
        return;
    }
    shutdownEngineClassRuntime(state);
    ludork::engine::runtime_detail::clearBlueprintRuntimeCaches();
    ludork::engine::runtime_detail::clearNodeGraphRuntimeCaches();
    ludork::runtime::shutdown(state);
}
