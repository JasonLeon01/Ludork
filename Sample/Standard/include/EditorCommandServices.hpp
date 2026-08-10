#pragma once

#include <StandardApi.hpp>

struct lua_State;

namespace ludork::standard {

using EditorCommandBoolControlHandler = void (*)(bool enabled);

LUDORK_STANDARD_API void registerEditorCommandEnvironment(lua_State* state,
                                                          const char* name,
                                                          int valueIndex);
LUDORK_STANDARD_API void unregisterEditorCommandEnvironment(
    lua_State* state, const char* name) noexcept;

LUDORK_STANDARD_API void registerEditorCommandInputHandler(lua_State* state,
                                                           int functionIndex);
LUDORK_STANDARD_API void unregisterEditorCommandInputHandler(
    lua_State* state) noexcept;

LUDORK_STANDARD_API void registerEditorCommandShutdownHandler(
    lua_State* state, int functionIndex);
LUDORK_STANDARD_API void unregisterEditorCommandShutdownHandler(
    lua_State* state) noexcept;

LUDORK_STANDARD_API void registerEditorCommandBoolControlHandler(
    lua_State* state, const char* name,
    EditorCommandBoolControlHandler handler);
LUDORK_STANDARD_API void unregisterEditorCommandBoolControlHandler(
    lua_State* state, const char* name) noexcept;

}  // namespace ludork::standard
