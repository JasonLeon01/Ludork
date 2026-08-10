#pragma once

struct lua_State;

namespace ludork::standard::runtime {

void initializeEditorConsole(lua_State* state, int jsonDecodeIndex);
void updateEditorConsole(lua_State* state);
void shutdownEditorConsole(lua_State* state) noexcept;

void setEditorCommandEnvironment(lua_State* state, const char* name,
                                 int valueIndex);
void clearEditorCommandEnvironment(lua_State* state, const char* name) noexcept;
void setEditorCommandInputHandler(lua_State* state, int functionIndex);
void clearEditorCommandInputHandler(lua_State* state) noexcept;
void setEditorCommandShutdownHandler(lua_State* state, int functionIndex);
void clearEditorCommandShutdownHandler(lua_State* state) noexcept;

}  // namespace ludork::standard::runtime
