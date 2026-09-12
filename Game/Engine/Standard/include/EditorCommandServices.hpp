#pragma once

#include <StandardApi.hpp>
#include <cstdint>
#include <string_view>

struct lua_State;

namespace ludork::standard {

inline constexpr std::int64_t EditorBridgeProtocolVersion = 1;

LUDORK_STANDARD_API std::uint64_t editorConnectionId();
LUDORK_STANDARD_API bool sendEditorMessage(std::uint64_t connectionId,
                                           std::string_view message);

using EditorCommandBoolControlHandler = void (*)(bool enabled);
using EditorCommandReloadHandler = void (*)(lua_State* state);

LUDORK_STANDARD_API void registerEditorCommandReloadHandler(
    lua_State* state, EditorCommandReloadHandler handler);
LUDORK_STANDARD_API void unregisterEditorCommandReloadHandler(
    lua_State* state) noexcept;

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

LUDORK_STANDARD_API void registerEditorLiveDebugHandler(lua_State* state,
                                                        int functionIndex);
LUDORK_STANDARD_API void unregisterEditorLiveDebugHandler(
    lua_State* state) noexcept;

LUDORK_STANDARD_API void registerEditorCommandBoolControlHandler(
    lua_State* state, const char* name,
    EditorCommandBoolControlHandler handler);
LUDORK_STANDARD_API void unregisterEditorCommandBoolControlHandler(
    lua_State* state, const char* name) noexcept;

}  // namespace ludork::standard
