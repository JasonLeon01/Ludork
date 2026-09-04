#include <EngineRuntimeServices.hpp>

#include <Gameplay/EngineClassRuntime.hpp>
#include <Gameplay/BlueprintRuntime/BlueprintRuntimeInternal.hpp>
#include <LudorkRuntimeBinding/NativeObjectCodec.hpp>
#include <NodeGraph/NodeGraphRuntime.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <RuntimeSession.hpp>
#include <Utils/EventBus.hpp>

#include <sol2/sol.hpp>

#include <stdexcept>

void initializeEngineRuntimeServices(lua_State* state) {
    if (state == nullptr) {
        return;
    }
    ludork::runtime::initialize(state);
    sol::state_view lua(state);
    ludork::engine::runtime_detail::clearBlueprintRuntimeCaches(lua);
    ludork::engine::runtime_detail::clearNodeGraphRuntimeCaches(lua);

    EventBus::setBlueprintEventValidator(
        [state](const RuntimeIdentityPtr& object,
                const std::string& eventName) {
            ludork::standard::LuaExecutionScope execution(state);
            if (!execution.active()) {
                throw std::runtime_error("Lua runtime session is stopping");
            }
            ludork::engine::runtime_detail::validateBlueprintEvent(
                sol::this_state(state),
                ludork::runtime::binding::writeLuaValue(sol::state_view(state),
                                                        object),
                eventName);
        });
    EventBus::setBlueprintEventInvoker([state](const RuntimeIdentityPtr& object,
                                               const std::string& eventName) {
        ludork::standard::LuaExecutionScope execution(state);
        if (!execution.active()) {
            throw std::runtime_error("Lua runtime session is stopping");
        }
        sol::state_view lua(state);
        const sol::object target =
            ludork::runtime::binding::writeLuaValue(lua, object);
        ludork::engine::runtime_detail::dispatchBlueprintEvent(
            sol::this_state(state), target,
            ludork::runtime::detail::classType(sol::this_state(state), target),
            eventName, sol::make_object(lua, lua.create_table()), {});
    });
}

void shutdownEngineRuntimeServices(lua_State* state) noexcept {
    EventBus::setBlueprintEventValidator({});
    EventBus::setBlueprintEventInvoker({});
    if (state == nullptr) {
        return;
    }
    sol::state_view lua(state);
    shutdownEngineClassRuntime(state);
    ludork::engine::runtime_detail::clearBlueprintRuntimeCaches(lua);
    ludork::engine::runtime_detail::clearNodeGraphRuntimeCaches(lua);
    ludork::runtime::shutdown(state);
}
