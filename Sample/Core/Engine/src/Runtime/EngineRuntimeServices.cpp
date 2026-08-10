#include <Runtime/EngineRuntimeServices.hpp>

#include "RuntimeSubsystemServices.hpp"

#include <ClassServices.hpp>
#include <LudorkCoreBinding.hpp>
#include <Runtime/EngineClassRuntime.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <RuntimeSession.hpp>
#include <Utils/EventBus.hpp>

#include <sol2/sol.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ServiceNames = const std::vector<std::string>&;

template <typename Callback>
void forEachServiceName(Callback&& callback) {
    callback(ludork::engine::runtime_detail::runtimeValueServiceNames());
    callback(ludork::engine::runtime_detail::metadataRuntimeServiceNames());
    callback(ludork::engine::runtime_detail::blueprintRuntimeServiceNames());
    callback(ludork::engine::runtime_detail::nodeGraphRuntimeServiceNames());
}

sol::table dispatchRuntimeService(sol::this_state state,
                                  const std::string& operation,
                                  const sol::table& arguments) {
    using namespace ludork::engine::runtime_detail;
    if (ServiceDispatchResult result =
            dispatchRuntimeValueService(state, operation, arguments)) {
        return *result;
    }
    if (ServiceDispatchResult result =
            dispatchMetadataRuntimeService(state, operation, arguments)) {
        return *result;
    }
    if (ServiceDispatchResult result =
            dispatchBlueprintRuntimeService(state, operation, arguments)) {
        return *result;
    }
    if (ServiceDispatchResult result =
            dispatchNodeGraphRuntimeService(state, operation, arguments)) {
        return *result;
    }
    return runtimeResolverResult(sol::state_view(state), {});
}

void registerRuntimeService(lua_State* state, const std::string& name) {
    sol::state_view lua(state);
    const sol::object rawCallback = sol::make_object(
        lua, sol::as_function([state, name](sol::variadic_args arguments)
                                  -> sol::variadic_results {
            ludork::standard::LuaExecutionScope execution(state);
            if (!execution.active()) {
                return {};
            }
            sol::state_view callbackLua(state);
            const sol::table packed =
                ludork::engine::runtime_detail::packArguments(callbackLua,
                                                              arguments);
            return ludork::engine::runtime_detail::unpackResults(
                callbackLua,
                dispatchRuntimeService(sol::this_state(state), name, packed));
        }));
    const sol::protected_function callback =
        rawCallback.as<sol::protected_function>();
    ludork::standard::class_runtime::registerService(lua, name, callback);
}

}  // namespace

void initializeEngineRuntimeServices(lua_State* state) {
    if (state == nullptr) {
        return;
    }
    sol::state_view lua(state);
    ludork::engine::runtime_detail::clearRuntimeServiceCaches(lua);
    forEachServiceName([state](ServiceNames names) {
        for (const std::string& name : names) {
            registerRuntimeService(state, name);
        }
    });

    EventBus::setBlueprintEventValidator(
        [state](const RuntimeIdentityPtr& object,
                const std::string& eventName) {
            ludork::standard::LuaExecutionScope execution(state);
            if (!execution.active()) {
                throw std::runtime_error("Lua runtime session is stopping");
            }
            ludork::engine::runtime_detail::validateBlueprintEvent(
                sol::this_state(state),
                ludork_core::writeLuaValue(sol::state_view(state), object),
                eventName);
        });
    EventBus::setBlueprintEventInvoker([state](const RuntimeIdentityPtr& object,
                                               const std::string& eventName) {
        ludork::standard::LuaExecutionScope execution(state);
        if (!execution.active()) {
            return;
        }
        sol::state_view lua(state);
        const sol::object target = ludork_core::writeLuaValue(lua, object);
        ludork::engine::runtime_detail::dispatchBlueprintEvent(
            sol::this_state(state), target,
            ludork::engine::runtime_detail::classType(sol::this_state(state),
                                                      target),
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
    forEachServiceName([lua](ServiceNames names) {
        for (const std::string& name : names) {
            ludork::standard::class_runtime::unregisterService(lua, name);
        }
    });
    ludork::engine::runtime_detail::clearRuntimeServiceCaches(lua);
}
