#include "ApplicationRuntime.hpp"

#include "ApplicationPlatform.hpp"

#include <EngineLifecycle.hpp>
#include <GlobalRuntimeApi.hpp>
#include <LuaError.hpp>
#include <LuaSF.hpp>
#include <RuntimeSession.hpp>
#include <Standard.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <filesystem>
#include <stdexcept>
#include <string>

extern "C" {
int luaopen_cjson(lua_State* state);
int luaopen_cjson_safe(lua_State* state);
}

namespace {

class RuntimeOwner {
public:
    explicit RuntimeOwner(lua_State* state) : state_(state) {}

    RuntimeOwner(const RuntimeOwner&) = delete;
    RuntimeOwner& operator=(const RuntimeOwner&) = delete;

    ~RuntimeOwner() {
        if (state_ == nullptr) {
            return;
        }
        ludork::standard::beginRuntimeShutdown(state_);
        ludork::standard::runRuntimeCleanups(state_);
        ludork::standard::shutdown(state_);
        lua_close(state_);
    }

private:
    lua_State* state_;
};

int initializeNativeModule(lua_State* state, const char* name,
                           lua_CFunction openFunction) {
    const int stackBase = lua_gettop(state);
    lua_pushcfunction(state, openFunction);
    if (ludork::standard::protectedLuaCall(state, 0, 1) != LUA_OK) {
        const std::string detail =
            ludork::application::detail::luaErrorMessage(state);
        lua_settop(state, stackBase);
        throw std::runtime_error("Failed to initialize native Lua module '" +
                                 std::string(name) + "': " + detail);
    }
    if (!lua_istable(state, -1)) {
        lua_settop(state, stackBase);
        throw std::runtime_error("Native Lua module '" + std::string(name) +
                                 "' did not return a table");
    }

    luaL_getsubtable(state, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_pushvalue(state, stackBase + 1);
    lua_setfield(state, -2, name);
    lua_pop(state, 1);
    return stackBase + 1;
}

void initializeRuntime(lua_State* state) {
    const int stackBase = lua_gettop(state);
    const int cjsonIndex =
        initializeNativeModule(state, "cjson", luaopen_cjson);
    static_cast<void>(
        initializeNativeModule(state, "cjson.safe", luaopen_cjson_safe));
    lua_pop(state, 1);
    try {
        ludork::standard::initialize(state, cjsonIndex);
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
    lua_settop(state, stackBase);
    ludork::standard::registerRuntimeCleanup(state, ludork::engine::shutdown);
    ludork::standard::registerRuntimeCleanup(state, ludork::global::shutdown);
}

}  // namespace

namespace ludork::application::detail {

std::string luaErrorMessage(lua_State* state) {
    std::size_t length = 0;
    const char* raw = luaL_tolstring(state, -1, &length);
    const std::string message =
        raw == nullptr ? "unknown Lua error" : std::string(raw, length);
    lua_pop(state, 1);
    return message;
}

int runRuntime(const std::filesystem::path& executablePath, int argc,
               char** argv) {
    lua_State* state = LuaSF_create_state();
    if (state == nullptr) {
        reportStartupError("Unable to create the Lua runtime state.");
        return 1;
    }
    RuntimeOwner runtime(state);

    registerRuntimeModules(state);
    initializeRuntime(state);
    configureApplicationScriptEnvironment(state, executablePath);
    return runApplicationScript(state, argc, argv);
}

}  // namespace ludork::application::detail
