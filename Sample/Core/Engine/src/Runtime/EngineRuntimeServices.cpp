#include <Runtime/EngineRuntimeServices.hpp>

#include "EngineRuntimeSession.hpp"
#include "RuntimeSubsystemServices.hpp"

#include <ClassServices.hpp>
#include <LudorkCoreBinding/NativeObjectCodec.hpp>
#include <Runtime/EngineClassRuntime.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <RuntimeSession.hpp>
#include <Utils/EventBus.hpp>

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <atomic>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ServiceNames = const std::vector<std::string>&;
std::atomic<lua_State*> engineRuntimeState{nullptr};

template <typename Callback>
void forEachServiceName(Callback&& callback) {
    callback(ludork::engine::runtime_detail::runtimeValueServiceNames());
    callback(ludork::engine::runtime_detail::metadataRuntimeServiceNames());
}

int dispatchRuntimeService(
    lua_State* state, const std::string& operation,
    const ludork::engine::runtime_detail::RuntimeArguments& arguments) {
    using namespace ludork::engine::runtime_detail;
    if (ServiceDispatchResult result = dispatchRuntimeValueService(
            sol::this_state(state), operation, arguments)) {
        return *result;
    }
    if (ServiceDispatchResult result = dispatchMetadataRuntimeService(
            sol::this_state(state), operation, arguments)) {
        return *result;
    }
    throw std::runtime_error("Engine runtime service '" + operation +
                             "' has no dispatcher");
}

int runtimeServiceCallback(lua_State* state) {
    try {
        ludork::standard::LuaExecutionScope execution(state);
        if (!execution.active()) {
            throw std::runtime_error("Lua runtime session is stopping");
        }
        std::size_t nameLength = 0;
        const char* rawName =
            lua_tolstring(state, lua_upvalueindex(1), &nameLength);
        if (rawName == nullptr) {
            return luaL_error(state, "%s", "Runtime service name is missing");
        }
        const std::string name(rawName, nameLength);
        const ludork::engine::runtime_detail::RuntimeArguments arguments(
            state, 1, lua_gettop(state));
        return dispatchRuntimeService(state, name, arguments);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    } catch (...) {
        return luaL_error(state, "%s", "Unknown runtime service error");
    }
}

void registerRuntimeService(lua_State* state, const std::string& name) {
    sol::state_view lua(state);
    lua_pushlstring(state, name.data(), name.size());
    lua_pushcclosure(state, &runtimeServiceCallback, 1);
    const sol::protected_function callback =
        sol::stack::get<sol::protected_function>(state, -1);
    lua_pop(state, 1);
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
            throw std::runtime_error("Lua runtime session is stopping");
        }
        sol::state_view lua(state);
        const sol::object target = ludork_core::writeLuaValue(lua, object);
        ludork::engine::runtime_detail::dispatchBlueprintEvent(
            sol::this_state(state), target,
            ludork::engine::runtime_detail::classType(sol::this_state(state),
                                                      target),
            eventName, sol::make_object(lua, lua.create_table()), {});
    });
    ludork::engine::runtime_detail::installEngineRuntimeState(state);
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
    ludork::engine::runtime_detail::clearEngineRuntimeState(state);
}

namespace ludork::engine::runtime_detail {

void installEngineRuntimeState(lua_State* state) {
    if (state == nullptr) {
        throw std::invalid_argument("Engine runtime state must not be null");
    }
    lua_State* expected = nullptr;
    if (!engineRuntimeState.compare_exchange_strong(
            expected, state, std::memory_order_acq_rel,
            std::memory_order_acquire) &&
        expected != state) {
        throw std::runtime_error("Engine runtime is already initialized");
    }
}

void clearEngineRuntimeState(lua_State* state) noexcept {
    lua_State* expected = state;
    engineRuntimeState.compare_exchange_strong(expected, nullptr,
                                               std::memory_order_release,
                                               std::memory_order_relaxed);
}

EngineRuntimeScope::EngineRuntimeScope()
    : state_(engineRuntimeState.load(std::memory_order_acquire)) {
    if (state_ == nullptr) {
        throw std::runtime_error("Engine runtime is not initialized");
    }
    execution_.emplace(state_);
    if (!execution_->active()) {
        throw std::runtime_error("Lua runtime session is stopping");
    }
}

sol::state_view EngineRuntimeScope::lua() const {
    return sol::state_view(state_);
}

lua_State* EngineRuntimeScope::state() const noexcept {
    return state_;
}

}  // namespace ludork::engine::runtime_detail
