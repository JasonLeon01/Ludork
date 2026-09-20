#include <Standard.hpp>
#include <JsonRuntimeProtocol.hpp>
#include <LudorkPlatform.hpp>
#include <LuaError.hpp>
#include <RuntimeSession.hpp>
#include <LuaSF.hpp>

#include "Bindings.hpp"
#include "Runtime/ClassRuntime/ClassRuntime.hpp"
#include "Runtime/EditorConsole.hpp"

#include <LuaGlue/LuaGlue.hpp>

#include <array>
#include <stdexcept>

namespace {

int updateFromLua(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        ludork::standard::update(state);
        return 0;
    });
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

int ownsLuaSFState(lua_State* state, void*) noexcept {
    return ludork::standard::ownsRuntimeSessionExecution(state);
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
    if (lua_glue::SetStateOwnsExecutionHook(state, ownsLuaSFState) != 0) {
        throw std::runtime_error(
            "Failed to install LuaGlue gate ownership hook");
    }
    lua_glue::StateView lua(state);
    lua["PLATFORM"] = LUDORK_PLATFORM;
    lua["SAVE_AS_LDC"] = LUDORK_SAVE_AS_LDC != 0;
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
    initializeMath(state);
    binding::registerString(lua);
    binding::registerTable(lua);
    lua_glue::Table cjson =
        lua_glue::Read<lua_glue::Table>(state, absoluteCjsonIndex);
    lua.registry().raw_set(
        ludork::standard::json_runtime::protocol::JSON_NULL_KEY,
        cjson.raw_get<lua_glue::Object>("null"));
    const lua_glue::Object jsonArrayMetatable =
        cjson.raw_get<lua_glue::Object>("array_mt");
    if (jsonArrayMetatable.get_type() != lua_glue::Type::Table) {
        throw std::runtime_error("cjson array metatable is not defined");
    }
    lua.registry().raw_set(
        ludork::standard::json_runtime::protocol::JSON_ARRAY_METATABLE_KEY,
        jsonArrayMetatable);
    const lua_glue::Object jsonEmptyArrayMetatable =
        cjson.raw_get<lua_glue::Object>("empty_array_mt");
    if (jsonEmptyArrayMetatable.get_type() != lua_glue::Type::Table) {
        throw std::runtime_error("cjson empty-array metatable is not defined");
    }
    lua.registry().raw_set(ludork::standard::json_runtime::protocol::
                               JSON_EMPTY_ARRAY_METATABLE_KEY,
                           jsonEmptyArrayMetatable);
    binding::registerContainers(lua);
    const lua_glue::Object jsonDecode =
        cjson.raw_get<lua_glue::Object>("decode");
    const lua_glue::Object jsonEncode =
        cjson.raw_get<lua_glue::Object>("encode");
    if (!jsonDecode.is<lua_glue::Function>()) {
        throw std::runtime_error("cjson decode function is not defined");
    }
    if (!jsonEncode.is<lua_glue::Function>()) {
        throw std::runtime_error("cjson encode function is not defined");
    }
    jsonDecode.push(state);
    jsonEncode.push(state);
    runtime::initializeEditorConsole(state, -2, -1);
    lua_pop(state, 2);
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
    binding::updateAsyncio(lua_glue::StateView(state));
}

void shutdown(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    beginRuntimeShutdown(state);
    {
        LuaExecutionScope execution(state);
        if (!execution.active()) {
            return;
        }
        state = execution.state();
    }
    runRuntimeCleanups(state);
    LuaSF_quiesce_state(state);
    {
        LuaExecutionScope execution(state);
        if (execution.active()) {
            state = execution.state();
            lua_glue::StateView lua(state);
            runtime::shutdownEditorConsole(state);
            binding::shutdownFileBatch(lua);
            binding::shutdownAsyncio(lua);
            binding::shutdownContainers(state);
            class_runtime::shutdown(state);
            clearRuntimeRegistryReferences(state);
            LuaSF_shutdown_state(state);
        }
    }
    releaseRuntimeSession(state);
}

}  // namespace ludork::standard
