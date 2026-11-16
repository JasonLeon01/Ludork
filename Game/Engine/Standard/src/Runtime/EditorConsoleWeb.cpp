#include "EditorConsole.hpp"

#include <EditorCommandServices.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace ludork::standard::runtime {

void initializeEditorConsole(lua_State*, int, int) {
    const char* commandPort = std::getenv("LUDORK_COMMAND_PORT");
    if (commandPort != nullptr && *commandPort != '\0') {
        std::cerr << "[EditorBridge] The editor console and Live Debug are "
                     "unavailable in Web builds. Use the browser console "
                     "for runtime output."
                  << std::endl;
    }
}

void updateEditorConsole(lua_State*) {}

void shutdownEditorConsole(lua_State*) noexcept {}

void setEditorCommandEnvironment(lua_State*, const char*, int) {}

void clearEditorCommandEnvironment(lua_State*, const char*) noexcept {}

void setEditorCommandInputHandler(lua_State*, int) {}

void clearEditorCommandInputHandler(lua_State*) noexcept {}

void setEditorCommandShutdownHandler(lua_State*, int) {}

void clearEditorCommandShutdownHandler(lua_State*) noexcept {}

}  // namespace ludork::standard::runtime

namespace ludork::standard {

std::uint64_t editorConnectionId() {
    return 0;
}

bool sendEditorMessage(std::uint64_t, std::string_view) {
    return false;
}

void registerEditorCommandReloadHandler(lua_State*,
                                        EditorCommandReloadHandler) {}

void unregisterEditorCommandReloadHandler(lua_State*) noexcept {}

void registerEditorCommandEnvironment(lua_State*, const char*, int) {}

void unregisterEditorCommandEnvironment(lua_State*, const char*) noexcept {}

void registerEditorCommandInputHandler(lua_State*, int) {}

void unregisterEditorCommandInputHandler(lua_State*) noexcept {}

void registerEditorCommandShutdownHandler(lua_State*, int) {}

void unregisterEditorCommandShutdownHandler(lua_State*) noexcept {}

void registerEditorLiveDebugHandler(lua_State*, int) {
    throw std::runtime_error("Live Debug is unavailable in Web builds.");
}

void unregisterEditorLiveDebugHandler(lua_State*) noexcept {}

void registerEditorCommandBoolControlHandler(lua_State*, const char*,
                                             EditorCommandBoolControlHandler) {}

void unregisterEditorCommandBoolControlHandler(lua_State*,
                                               const char*) noexcept {}

}  // namespace ludork::standard
