#pragma once

#include <filesystem>
#include <string>

struct lua_State;

namespace ludork::application::detail {

std::string luaErrorMessage(lua_State* state);
void registerRuntimeModules(lua_State* state);
void configureApplicationScriptEnvironment(
    lua_State* state, const std::filesystem::path& executablePath);
int runApplicationScript(lua_State* state, int argc, char** argv);
int runRuntime(const std::filesystem::path& executablePath, int argc,
               char** argv);

}  // namespace ludork::application::detail
