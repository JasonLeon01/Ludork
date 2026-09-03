#include <Standard.hpp>
#include <LudorkPlatform.hpp>
#include <LuaError.hpp>
#include <RuntimeSession.hpp>
#include <LuaSF.hpp>

#include "Bindings.hpp"
#include "Runtime/ClassRuntime/ClassRuntime.hpp"
#include "Runtime/EditorConsole.hpp"

#include <sol2/sol.hpp>

#include <array>
#include <stdexcept>

namespace {

int updateFromLua(lua_State* state) {
    ludork::standard::update(state);
    return 0;
}

int enterLuaSFState(lua_State* state, void*) noexcept {
    return ludork::standard::enterRuntimeSession(state);
}

int tryEnterLuaSFState(lua_State* state, void*) noexcept {
    return ludork::standard::tryEnterRuntimeSession(state);
}

void leaveLuaSFState(lua_State* state, void*) noexcept {
    ludork::standard::leaveRuntimeSession(state);
}

}  // namespace

namespace ludork::standard {

void initialize(lua_State* state, int cjsonIndex) {
    if (state == nullptr) {
        return;
    }
    const int absoluteCjsonIndex = lua_absindex(state, cjsonIndex);
    if (!lua_istable(state, absoluteCjsonIndex)) {
        throw std::invalid_argument("cjson module must be a table");
    }
    installLuaErrorHandler(state);
    initializeRuntimeSession(state);
    if (LuaSF_set_state_execution_hooks(state, enterLuaSFState,
                                        tryEnterLuaSFState, leaveLuaSFState,
                                        nullptr) != 0) {
        throw std::runtime_error("Failed to install LuaSF execution hooks");
    }
    sol::state_view lua(state);
    lua["PLATFORM"] = LUDORK_PLATFORM;
#if defined(LUDORK_MOBILE)
    lua["LUDORK_MOBILE"] = true;
    lua["LUDORK_DESKTOP"] = false;
#else
    lua["LUDORK_MOBILE"] = false;
    lua["LUDORK_DESKTOP"] = true;
#endif
    binding::registerClass(lua);
    binding::registerConfigParser(lua);
    binding::registerCodecs(lua);
    binding::registerSystemServices(lua);
    binding::registerAsyncio(lua);
    binding::registerFileBatch(lua);
    binding::registerString(lua);
    binding::registerTable(lua);
    sol::table cjson = sol::stack::get<sol::table>(state, absoluteCjsonIndex);
    lua.registry().raw_set("LuaSF.JsonNullSentinel",
                           cjson.raw_get<sol::object>("null"));
    const sol::object jsonArrayMetatable =
        cjson.raw_get<sol::object>("array_mt");
    if (jsonArrayMetatable.get_type() != sol::type::table) {
        throw std::runtime_error("cjson array metatable is not defined");
    }
    lua.registry().raw_set("LuaSF.JsonArrayMetatable", jsonArrayMetatable);
    const sol::object jsonEmptyArrayMetatable =
        cjson.raw_get<sol::object>("empty_array_mt");
    if (jsonEmptyArrayMetatable.get_type() != sol::type::table) {
        throw std::runtime_error("cjson empty-array metatable is not defined");
    }
    lua.registry().raw_set("LuaSF.JsonEmptyArrayMetatable",
                           jsonEmptyArrayMetatable);
    binding::registerContainers(lua);
    const sol::object jsonDecode = cjson.raw_get<sol::object>("decode");
    if (!jsonDecode.is<sol::protected_function>()) {
        throw std::runtime_error("cjson decode function is not defined");
    }
    jsonDecode.push();
    runtime::initializeEditorConsole(state, -1);
    lua_pop(state, 1);
    lua_pushcfunction(state, updateFromLua);
    lua_setglobal(state, "_LUDORK_STANDARD_UPDATE");
}

void update(lua_State* state) {
    if (state == nullptr) {
        return;
    }
    std::array<char, 512> callbackError{};
    if (LuaSF_take_deferred_callback_error(state, callbackError.data(),
                                           callbackError.size()) != 0) {
        throw std::runtime_error(callbackError.data());
    }
    runtime::updateEditorConsole(state);
    binding::updateAsyncio(sol::state_view(state));
}

void shutdown(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    runRuntimeCleanups(state);
    LuaSF_quiesce_state(state);
    LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    sol::state_view lua(state);
    runtime::shutdownEditorConsole(state);
    binding::shutdownFileBatch(lua);
    binding::shutdownAsyncio(lua);
    binding::shutdownContainers(state);
    class_runtime::shutdown(state);
    clearRuntimeRegistryReferences(state);
    LuaSF_shutdown_state(state);
    releaseRuntimeSession(state);
}

}  // namespace ludork::standard
