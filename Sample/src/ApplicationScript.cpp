#include "ApplicationRuntime.hpp"

#include "ApplicationPaths.hpp"
#include "ApplicationPlatform.hpp"

#include <ConfigParser.hpp>
#include <LuaError.hpp>
#include <RuntimeSession.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <filesystem>
#include <optional>
#include <string>

namespace {

void configureLuaSearchPaths(lua_State* state,
                             const std::filesystem::path& executablePath) {
    lua_getglobal(state, "package");
    lua_getfield(state, -1, "path");
    const char* packagePath = lua_tostring(state, -1);
    const std::string scriptModulePath = "Scripts/?.lua;Scripts/?.luac;";
    lua_pushlstring(state, scriptModulePath.c_str(), scriptModulePath.size());
    lua_pushstring(state, packagePath == nullptr ? "" : packagePath);
    lua_concat(state, 2);
    lua_setfield(state, -3, "path");
    lua_pop(state, 2);

    lua_getglobal(state, "package");
    lua_getfield(state, -1, "cpath");
    const char* cpackagePath = lua_tostring(state, -1);
    const std::string executableDirectory =
        executablePath.parent_path().generic_string();
    const std::string nativeModulePath =
        executableDirectory + "/?.dll;" + executableDirectory + "/?.so;" +
        executableDirectory + "/?.dylib;?.dll;?.so;?.dylib;";
    lua_pushlstring(state, nativeModulePath.c_str(), nativeModulePath.size());
    lua_pushstring(state, cpackagePath == nullptr ? "" : cpackagePath);
    lua_concat(state, 2);
    lua_setfield(state, -3, "cpath");
    lua_pop(state, 2);
}

void setLuaArguments(lua_State* state, int argc, char** argv) {
    lua_createtable(state, argc > 2 ? argc - 2 : 0, 1);
    for (int index = 1; index < argc; ++index) {
        lua_pushstring(state, argv[index]);
        lua_rawseti(state, -2, index - 1);
    }
    lua_setglobal(state, "arg");
}

std::string configuredScriptPath() {
    ludork::standard::ConfigParser iniFile;
    if (!iniFile.read("Main.ini")) {
        return "Scripts/Entry.lua";
    }
    const std::optional<std::string> script = iniFile.get("Main", "script");
    return script.has_value() && !script->empty() ? *script
                                                  : "Scripts/Entry.lua";
}

int runEntryScript(lua_State* state, const std::filesystem::path& scriptPath) {
    ludork::standard::LuaExecutionScope scriptExecution(state);
    if (!scriptExecution.active()) {
        return 1;
    }
    const std::filesystem::path resolvedPath =
        ludork::application::detail::resolveLuaScriptPath(scriptPath);
    const std::string resolvedPathText = resolvedPath.generic_string();
    if (luaL_loadfile(state, resolvedPathText.c_str()) != LUA_OK) {
        ludork::application::detail::reportStartupError(
            "Lua error while loading " + resolvedPathText + ": " +
            ludork::application::detail::luaErrorMessage(state));
        return 1;
    }
    if (ludork::standard::protectedLuaCall(state, 0, LUA_MULTRET) == LUA_OK) {
        return 0;
    }
    ludork::application::detail::reportStartupError(
        "Lua error while loading " + resolvedPathText + ": " +
        ludork::application::detail::luaErrorMessage(state));
    return 1;
}

}  // namespace

namespace ludork::application::detail {

void configureApplicationScriptEnvironment(
    lua_State* state, const std::filesystem::path& executablePath) {
    configureLuaSearchPaths(state, executablePath);
}

int runApplicationScript(lua_State* state, int argc, char** argv) {
    setLuaArguments(state, argc, argv);
    const std::string scriptPath = argc > 1 ? argv[1] : configuredScriptPath();
    return runEntryScript(state, scriptPath);
}

}  // namespace ludork::application::detail
