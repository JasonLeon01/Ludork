#include "EditorConsole.hpp"

#include <EditorCommandServices.hpp>
#include <LuaError.hpp>
#include <RuntimeSession.hpp>

#include <SFML/Network/IpAddress.hpp>
#include <SFML/Network/Socket.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <array>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>

namespace ludork::standard::runtime {
namespace {

struct EditorConsoleRuntime {
    lua_State* state{};
    sf::TcpListener listener;
    sf::TcpSocket client;
    std::string input;
    int environmentReference{LUA_NOREF};
    int jsonDecodeReference{LUA_NOREF};
    int inputInjectReference{LUA_NOREF};
    int shutdownReference{LUA_NOREF};
    std::unordered_map<std::string, EditorCommandBoolControlHandler>
        boolControlHandlers;
    bool connected{};
};

std::unique_ptr<EditorConsoleRuntime> editorConsole;
constexpr lua_Integer protocolVersion = 2;
constexpr std::size_t maximumMessageSize = 64 * 1024;

std::optional<unsigned short> readCommandPort() {
    const char* rawPort = std::getenv("LUDORK_COMMAND_PORT");
    if (rawPort == nullptr || *rawPort == '\0') {
        return std::nullopt;
    }
    unsigned int port{};
    const char* end = rawPort + std::char_traits<char>::length(rawPort);
    std::from_chars_result result = std::from_chars(rawPort, end, port);
    if (result.ec != std::errc{} || result.ptr != end || port == 0 ||
        port > 65535) {
        return std::nullopt;
    }
    return static_cast<unsigned short>(port);
}

void writeLuaError(lua_State* state) {
    const char* message = lua_tostring(state, -1);
    std::cerr << "[Console] "
              << (message == nullptr ? "unknown Lua error" : message)
              << std::endl;
}

bool ensureEnvironment(lua_State* state, EditorConsoleRuntime& runtime) {
    if (runtime.environmentReference != LUA_NOREF) {
        return true;
    }

    lua_newtable(state);
    int environmentIndex = lua_absindex(state, -1);
    lua_newtable(state);
    lua_pushglobaltable(state);
    lua_setfield(state, -2, "__index");
    lua_setmetatable(state, environmentIndex);
    runtime.environmentReference = luaL_ref(state, LUA_REGISTRYINDEX);
    return true;
}

bool decodeMessage(lua_State* state, EditorConsoleRuntime& runtime,
                   const std::string& line) {
    if (runtime.jsonDecodeReference == LUA_NOREF) {
        return false;
    }
    lua_rawgeti(state, LUA_REGISTRYINDEX, runtime.jsonDecodeReference);
    lua_pushlstring(state, line.data(), line.size());
    if (ludork::standard::protectedLuaCall(state, 1, 1) != LUA_OK) {
        writeLuaError(state);
        lua_pop(state, 1);
        return false;
    }
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        std::cerr << "[EditorBridge] Message must be a JSON object."
                  << std::endl;
        return false;
    }
    return true;
}

bool setChunkEnvironment(lua_State* state, int environmentReference) {
    lua_rawgeti(state, LUA_REGISTRYINDEX, environmentReference);
    const char* upvalueName = lua_setupvalue(state, -2, 1);
    if (upvalueName != nullptr) {
        return true;
    }
    lua_pop(state, 1);
    std::cerr << "[Console] Lua chunk has no environment." << std::endl;
    return false;
}

bool appendResult(lua_State* state, int resultIndex, std::string& output) {
    lua_getglobal(state, "tostring");
    lua_pushvalue(state, resultIndex);
    if (ludork::standard::protectedLuaCall(state, 1, 1) != LUA_OK) {
        writeLuaError(state);
        lua_pop(state, 1);
        return false;
    }
    std::size_t length{};
    const char* value = lua_tolstring(state, -1, &length);
    if (value != nullptr) {
        output.append(value, length);
    }
    lua_pop(state, 1);
    return true;
}

void executeCommand(lua_State* state, EditorConsoleRuntime& runtime,
                    const std::string& command) {
    int stackBase = lua_gettop(state);
    if (!ensureEnvironment(state, runtime)) {
        lua_settop(state, stackBase);
        return;
    }

    std::string expression = "return " + command;
    bool expressionMode = true;
    int status = luaL_loadbufferx(state, expression.data(), expression.size(),
                                  "=(console)", "t");
    if (status != LUA_OK) {
        lua_pop(state, 1);
        expressionMode = false;
        status = luaL_loadbufferx(state, command.data(), command.size(),
                                  "=(console)", "t");
    }
    if (status != LUA_OK) {
        writeLuaError(state);
        lua_settop(state, stackBase);
        return;
    }
    if (!setChunkEnvironment(state, runtime.environmentReference)) {
        lua_settop(state, stackBase);
        return;
    }

    status = ludork::standard::protectedLuaCall(state, 0, LUA_MULTRET);
    if (status != LUA_OK) {
        writeLuaError(state);
        lua_settop(state, stackBase);
        return;
    }

    int resultTop = lua_gettop(state);
    if (expressionMode && resultTop > stackBase) {
        std::string output;
        for (int index = stackBase + 1; index <= resultTop; ++index) {
            if (index > stackBase + 1) {
                output.push_back('\t');
            }
            if (!appendResult(state, index, output)) {
                lua_settop(state, stackBase);
                return;
            }
        }
        std::cout << output << std::endl;
    }
    lua_settop(state, stackBase);
}

bool injectInputEvents(lua_State* state, EditorConsoleRuntime& runtime,
                       int messageIndex) {
    if (runtime.inputInjectReference == LUA_NOREF) {
        std::cerr << "[EditorBridge] No input handler is registered."
                  << std::endl;
        return false;
    }

    lua_getfield(state, messageIndex, "events");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        std::cerr << "[EditorBridge] Input message requires an events array."
                  << std::endl;
        return false;
    }
    int eventsIndex = lua_absindex(state, -1);
    std::size_t eventCount = lua_rawlen(state, eventsIndex);
    for (std::size_t index = 1; index <= eventCount; ++index) {
        lua_rawgeti(state, eventsIndex, static_cast<lua_Integer>(index));
        if (!lua_istable(state, -1)) {
            lua_pop(state, 1);
            std::cerr << "[EditorBridge] Input event must be a JSON object."
                      << std::endl;
            continue;
        }
        lua_rawgeti(state, LUA_REGISTRYINDEX, runtime.inputInjectReference);
        lua_insert(state, -2);
        if (ludork::standard::protectedLuaCall(state, 1, 0) != LUA_OK) {
            writeLuaError(state);
            lua_pop(state, 1);
            lua_pop(state, 1);
            return false;
        }
    }
    lua_pop(state, 1);
    return true;
}

bool requestShutdown(lua_State* state, EditorConsoleRuntime& runtime) {
    if (runtime.shutdownReference == LUA_NOREF) {
        std::cerr << "[EditorBridge] No shutdown handler is registered."
                  << std::endl;
        return false;
    }
    lua_rawgeti(state, LUA_REGISTRYINDEX, runtime.shutdownReference);
    if (ludork::standard::protectedLuaCall(state, 0, 0) != LUA_OK) {
        writeLuaError(state);
        lua_pop(state, 1);
        return false;
    }
    return true;
}

void applyBoolControl(lua_State* state, EditorConsoleRuntime& runtime,
                      int messageIndex) {
    lua_getfield(state, messageIndex, "name");
    std::size_t nameLength{};
    const char* rawName = lua_type(state, -1) == LUA_TSTRING
                              ? lua_tolstring(state, -1, &nameLength)
                              : nullptr;
    std::string name =
        rawName == nullptr ? std::string{} : std::string(rawName, nameLength);
    lua_pop(state, 1);
    if (name.empty()) {
        std::cerr << "[EditorBridge] Control message requires a name."
                  << std::endl;
        return;
    }

    lua_getfield(state, messageIndex, "enabled");
    if (!lua_isboolean(state, -1)) {
        lua_pop(state, 1);
        std::cerr << "[EditorBridge] Bool control requires enabled."
                  << std::endl;
        return;
    }
    bool enabled = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);

    auto handler = runtime.boolControlHandlers.find(name);
    if (handler == runtime.boolControlHandlers.end()) {
        std::cerr << "[EditorBridge] Unknown bool control: " << name
                  << std::endl;
        return;
    }
    handler->second(enabled);
}

bool processMessage(lua_State* state, EditorConsoleRuntime& runtime,
                    const std::string& line) {
    int stackBase = lua_gettop(state);
    if (!decodeMessage(state, runtime, line)) {
        lua_settop(state, stackBase);
        return true;
    }

    int messageIndex = lua_absindex(state, -1);
    lua_getfield(state, messageIndex, "v");
    bool versionValid = lua_type(state, -1) == LUA_TNUMBER &&
                        lua_tonumber(state, -1) == protocolVersion;
    lua_pop(state, 1);
    if (!versionValid) {
        std::cerr << "[EditorBridge] Protocol version mismatch." << std::endl;
        lua_settop(state, stackBase);
        return false;
    }

    lua_getfield(state, messageIndex, "type");
    const char* rawType = lua_tostring(state, -1);
    std::string_view type = rawType == nullptr ? std::string_view{} : rawType;
    lua_pop(state, 1);

    if (type == "command") {
        lua_getfield(state, messageIndex, "command");
        std::size_t commandLength{};
        const char* command = lua_type(state, -1) == LUA_TSTRING
                                  ? lua_tolstring(state, -1, &commandLength)
                                  : nullptr;
        if (command != nullptr) {
            executeCommand(state, runtime, std::string(command, commandLength));
        } else {
            std::cerr << "[EditorBridge] Command message requires text."
                      << std::endl;
        }
        lua_pop(state, 1);
    } else if (type == "input") {
        injectInputEvents(state, runtime, messageIndex);
    } else if (type == "shutdown") {
        requestShutdown(state, runtime);
    } else if (type == "control") {
        applyBoolControl(state, runtime, messageIndex);
    } else {
        std::cerr << "[EditorBridge] Unknown message type." << std::endl;
    }

    lua_settop(state, stackBase);
    return true;
}

bool processMessages(lua_State* state, EditorConsoleRuntime& runtime) {
    std::size_t newline = runtime.input.find('\n');
    while (newline != std::string::npos) {
        std::string line = runtime.input.substr(0, newline);
        runtime.input.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.size() > maximumMessageSize) {
            std::cerr << "[EditorBridge] Message exceeds the size limit."
                      << std::endl;
            return false;
        }
        if (!line.empty() && !processMessage(state, runtime, line)) {
            return false;
        }
        newline = runtime.input.find('\n');
    }
    if (runtime.input.size() > maximumMessageSize) {
        std::cerr << "[EditorBridge] Message exceeds the size limit."
                  << std::endl;
        return false;
    }
    return true;
}

void disconnectClient(EditorConsoleRuntime& runtime) {
    runtime.client.disconnect();
    runtime.connected = false;
    runtime.input.clear();
}

bool sendReady(EditorConsoleRuntime& runtime) {
    static constexpr std::string_view ready = "{\"v\":2,\"type\":\"ready\"}\n";
    return runtime.client.send(ready.data(), ready.size()) ==
           sf::Socket::Status::Done;
}

}  // namespace

void initializeEditorConsole(lua_State* state, int jsonDecodeIndex) {
    if (editorConsole) {
        shutdownEditorConsole(state);
    }

    const char* rawPort = std::getenv("LUDORK_COMMAND_PORT");
    if (rawPort == nullptr || *rawPort == '\0') {
        return;
    }
    std::optional<unsigned short> port = readCommandPort();
    if (!port) {
        std::cerr << "[Console] Invalid LUDORK_COMMAND_PORT." << std::endl;
        return;
    }
    if (!lua_isfunction(state, jsonDecodeIndex)) {
        std::cerr << "[Console] JSON decoder is not callable." << std::endl;
        return;
    }

    std::unique_ptr<EditorConsoleRuntime> runtime =
        std::make_unique<EditorConsoleRuntime>();
    sf::Socket::Status status =
        runtime->listener.listen(*port, sf::IpAddress::LocalHostV4);
    if (status != sf::Socket::Status::Done) {
        std::cerr << "[Console] Failed to listen on 127.0.0.1:" << *port
                  << std::endl;
        return;
    }
    runtime->listener.setBlocking(false);
    runtime->state = state;
    lua_pushvalue(state, jsonDecodeIndex);
    runtime->jsonDecodeReference = luaL_ref(state, LUA_REGISTRYINDEX);
    editorConsole = std::move(runtime);
}

void updateEditorConsole(lua_State* state) {
    if (!editorConsole) {
        return;
    }
    EditorConsoleRuntime& runtime = *editorConsole;

    if (!runtime.connected) {
        sf::Socket::Status acceptStatus =
            runtime.listener.accept(runtime.client);
        if (acceptStatus != sf::Socket::Status::Done) {
            return;
        }
        runtime.connected = true;
        runtime.input.clear();
        if (!sendReady(runtime)) {
            disconnectClient(runtime);
            return;
        }
        runtime.client.setBlocking(false);
    }

    std::array<char, 4096> buffer{};
    while (runtime.connected) {
        std::size_t received{};
        sf::Socket::Status status =
            runtime.client.receive(buffer.data(), buffer.size(), received);
        if (status == sf::Socket::Status::Done) {
            if (received == 0) {
                break;
            }
            runtime.input.append(buffer.data(), received);
            if (!processMessages(state, runtime)) {
                disconnectClient(runtime);
                break;
            }
            continue;
        }
        if (status == sf::Socket::Status::NotReady) {
            break;
        }
        disconnectClient(runtime);
    }
}

void shutdownEditorConsole(lua_State* state) noexcept {
    if (!editorConsole || editorConsole->state != state) {
        return;
    }
    if (editorConsole->environmentReference != LUA_NOREF) {
        luaL_unref(state, LUA_REGISTRYINDEX,
                   editorConsole->environmentReference);
        editorConsole->environmentReference = LUA_NOREF;
    }
    if (editorConsole->jsonDecodeReference != LUA_NOREF) {
        luaL_unref(state, LUA_REGISTRYINDEX,
                   editorConsole->jsonDecodeReference);
        editorConsole->jsonDecodeReference = LUA_NOREF;
    }
    if (editorConsole->inputInjectReference != LUA_NOREF) {
        luaL_unref(state, LUA_REGISTRYINDEX,
                   editorConsole->inputInjectReference);
        editorConsole->inputInjectReference = LUA_NOREF;
    }
    if (editorConsole->shutdownReference != LUA_NOREF) {
        luaL_unref(state, LUA_REGISTRYINDEX, editorConsole->shutdownReference);
        editorConsole->shutdownReference = LUA_NOREF;
    }
    disconnectClient(*editorConsole);
    editorConsole->listener.close();
    editorConsole.reset();
}

void setEditorCommandEnvironment(lua_State* state, const char* name,
                                 int valueIndex) {
    if (!editorConsole || editorConsole->state != state || name == nullptr ||
        *name == '\0' || lua_isnoneornil(state, valueIndex)) {
        return;
    }
    const int absoluteValueIndex = lua_absindex(state, valueIndex);
    if (!ensureEnvironment(state, *editorConsole)) {
        return;
    }
    lua_rawgeti(state, LUA_REGISTRYINDEX, editorConsole->environmentReference);
    lua_pushvalue(state, absoluteValueIndex);
    lua_setfield(state, -2, name);
    lua_pop(state, 1);
}

void clearEditorCommandEnvironment(lua_State* state,
                                   const char* name) noexcept {
    if (!editorConsole || editorConsole->state != state || name == nullptr ||
        *name == '\0' || editorConsole->environmentReference == LUA_NOREF) {
        return;
    }
    lua_rawgeti(state, LUA_REGISTRYINDEX, editorConsole->environmentReference);
    lua_pushnil(state);
    lua_setfield(state, -2, name);
    lua_pop(state, 1);
}

void setEditorCommandInputHandler(lua_State* state, int functionIndex) {
    if (!editorConsole || editorConsole->state != state ||
        !lua_isfunction(state, functionIndex)) {
        return;
    }
    if (editorConsole->inputInjectReference != LUA_NOREF) {
        luaL_unref(state, LUA_REGISTRYINDEX,
                   editorConsole->inputInjectReference);
    }
    lua_pushvalue(state, functionIndex);
    editorConsole->inputInjectReference = luaL_ref(state, LUA_REGISTRYINDEX);
}

void clearEditorCommandInputHandler(lua_State* state) noexcept {
    if (!editorConsole || editorConsole->state != state ||
        editorConsole->inputInjectReference == LUA_NOREF) {
        return;
    }
    luaL_unref(state, LUA_REGISTRYINDEX, editorConsole->inputInjectReference);
    editorConsole->inputInjectReference = LUA_NOREF;
}

void setEditorCommandShutdownHandler(lua_State* state, int functionIndex) {
    if (!editorConsole || editorConsole->state != state ||
        !lua_isfunction(state, functionIndex)) {
        return;
    }
    if (editorConsole->shutdownReference != LUA_NOREF) {
        luaL_unref(state, LUA_REGISTRYINDEX, editorConsole->shutdownReference);
    }
    lua_pushvalue(state, functionIndex);
    editorConsole->shutdownReference = luaL_ref(state, LUA_REGISTRYINDEX);
}

void clearEditorCommandShutdownHandler(lua_State* state) noexcept {
    if (!editorConsole || editorConsole->state != state ||
        editorConsole->shutdownReference == LUA_NOREF) {
        return;
    }
    luaL_unref(state, LUA_REGISTRYINDEX, editorConsole->shutdownReference);
    editorConsole->shutdownReference = LUA_NOREF;
}

void setEditorCommandBoolControlHandler(
    lua_State* state, const char* name,
    EditorCommandBoolControlHandler handler) {
    if (!editorConsole || editorConsole->state != state || name == nullptr ||
        *name == '\0' || handler == nullptr) {
        return;
    }
    editorConsole->boolControlHandlers[name] = handler;
}

void clearEditorCommandBoolControlHandler(lua_State* state,
                                          const char* name) noexcept {
    if (!editorConsole || editorConsole->state != state || name == nullptr ||
        *name == '\0') {
        return;
    }
    editorConsole->boolControlHandlers.erase(name);
}

}  // namespace ludork::standard::runtime

namespace ludork::standard {

void registerEditorCommandEnvironment(lua_State* state, const char* name,
                                      int valueIndex) {
    LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    runtime::setEditorCommandEnvironment(state, name, valueIndex);
}

void unregisterEditorCommandEnvironment(lua_State* state,
                                        const char* name) noexcept {
    LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    runtime::clearEditorCommandEnvironment(state, name);
}

void registerEditorCommandInputHandler(lua_State* state, int functionIndex) {
    LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    runtime::setEditorCommandInputHandler(state, functionIndex);
}

void unregisterEditorCommandInputHandler(lua_State* state) noexcept {
    LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    runtime::clearEditorCommandInputHandler(state);
}

void registerEditorCommandShutdownHandler(lua_State* state, int functionIndex) {
    LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    runtime::setEditorCommandShutdownHandler(state, functionIndex);
}

void unregisterEditorCommandShutdownHandler(lua_State* state) noexcept {
    LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    runtime::clearEditorCommandShutdownHandler(state);
}

void registerEditorCommandBoolControlHandler(
    lua_State* state, const char* name,
    EditorCommandBoolControlHandler handler) {
    LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    runtime::setEditorCommandBoolControlHandler(state, name, handler);
}

void unregisterEditorCommandBoolControlHandler(lua_State* state,
                                               const char* name) noexcept {
    LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    runtime::clearEditorCommandBoolControlHandler(state, name);
}

}  // namespace ludork::standard
