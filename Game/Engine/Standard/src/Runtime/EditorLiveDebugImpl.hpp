#pragma once

struct lua_State;

namespace ludork::standard::runtime {

struct EditorConsoleImpl;

void processEditorLiveDebugMessage(lua_State* state, EditorConsoleImpl& runtime,
                                   int messageIndex);

}  // namespace ludork::standard::runtime
