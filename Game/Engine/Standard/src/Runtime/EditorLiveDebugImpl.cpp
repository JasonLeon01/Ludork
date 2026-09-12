#include "EditorLiveDebugImpl.hpp"

#include "EditorConsoleImpl.hpp"

#include <Core/Utf8.hpp>
#include <EditorCommandServices.hpp>
#include <LuaError.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ludork::standard::runtime {
namespace {

constexpr std::size_t chunkSize = 8 * 1024;
constexpr std::size_t maximumPendingSize = 32 * 1024 * 1024;
constexpr std::size_t maximumIdentifierSize = 128;

std::string readString(lua_State* state, int index, const char* key) {
    lua_getfield(state, index, key);
    std::size_t length{};
    const char* value = lua_type(state, -1) == LUA_TSTRING
                            ? lua_tolstring(state, -1, &length)
                            : nullptr;
    if (value == nullptr || length == 0 || length > maximumIdentifierSize) {
        lua_pop(state, 1);
        throw std::invalid_argument(
            std::string("Live debug ") + key +
            " must be a non-empty string of at most 128 bytes");
    }
    std::string result(value, length);
    lua_pop(state, 1);
    ludork::standard::detail::validateUtf8(result, key);
    return result;
}

std::string encodeValue(lua_State* state, EditorConsoleImpl& runtime,
                        int index) {
    const int valueIndex = lua_absindex(state, index);
    lua_rawgeti(state, LUA_REGISTRYINDEX, runtime.jsonEncodeReference);
    lua_pushvalue(state, valueIndex);
    if (ludork::standard::protectedLuaCall(state, 1, 1) != LUA_OK) {
        const char* error = lua_tostring(state, -1);
        const std::string message =
            error == nullptr ? "JSON encoding failed" : error;
        lua_pop(state, 1);
        throw std::runtime_error(message);
    }
    std::size_t length{};
    const char* text = lua_tolstring(state, -1, &length);
    if (text == nullptr) {
        lua_pop(state, 1);
        throw std::runtime_error("Live debug JSON encoder did not return text");
    }
    std::string result(text, length);
    lua_pop(state, 1);
    return result;
}

void setString(lua_State* state, const char* key, std::string_view value) {
    lua_pushlstring(state, value.data(), value.size());
    lua_setfield(state, -2, key);
}

void queueResponse(lua_State* state, EditorConsoleImpl& runtime,
                   const std::string& requestId, const std::string& run,
                   const std::string& payload) {
    ludork::standard::detail::validateUtf8(payload, "Live debug response");
    if (payload.size() > maximumPendingSize) {
        throw std::runtime_error(
            "Live debug response exceeds the 32 MiB limit");
    }
    std::vector<std::string_view> chunks;
    std::size_t offset = 0;
    while (offset < payload.size()) {
        std::size_t end = std::min(payload.size(), offset + chunkSize);
        if (end < payload.size()) {
            while ((static_cast<unsigned char>(payload[end]) & 0xC0) == 0x80) {
                --end;
            }
        }
        chunks.emplace_back(payload.data() + offset, end - offset);
        offset = end;
    }
    std::vector<std::string> messages;
    std::size_t totalSize = 0;
    for (std::size_t index = 0; index < chunks.size(); ++index) {
        lua_createtable(state, 0, 7);
        lua_pushinteger(state, EditorBridgeProtocolVersion);
        lua_setfield(state, -2, "v");
        setString(state, "type", "liveDebug");
        setString(state, "requestId", requestId);
        setString(state, "run", run);
        lua_pushinteger(state, static_cast<lua_Integer>(index));
        lua_setfield(state, -2, "chunkIndex");
        lua_pushinteger(state, static_cast<lua_Integer>(chunks.size()));
        lua_setfield(state, -2, "chunkCount");
        setString(state, "payload", chunks[index]);
        messages.push_back(encodeValue(state, runtime, -1));
        lua_pop(state, 1);
        totalSize += messages.back().size() + 1;
        if (runtime.liveDebugOutputSize + totalSize > maximumPendingSize) {
            throw std::runtime_error(
                "Live debug output exceeds the 32 MiB limit");
        }
    }
    for (std::string& message : messages) {
        runtime.liveDebugOutput.emplace_back(std::move(message));
    }
    runtime.liveDebugOutputSize += totalSize;
}

void queueFailure(lua_State* state, EditorConsoleImpl& runtime,
                  const std::string& requestId, const std::string& run,
                  std::string_view error) {
    lua_createtable(state, 0, 2);
    lua_pushboolean(state, false);
    lua_setfield(state, -2, "success");
    setString(state, "error", error);
    const std::string payload = encodeValue(state, runtime, -1);
    lua_pop(state, 1);
    queueResponse(state, runtime, requestId, run, payload);
}

}  // namespace

void processEditorLiveDebugMessage(lua_State* state, EditorConsoleImpl& runtime,
                                   int messageIndex) {
    const int stackBase = lua_gettop(state);
    messageIndex = lua_absindex(state, messageIndex);
    std::string requestId;
    std::string run;
    try {
        requestId = readString(state, messageIndex, "requestId");
        run = readString(state, messageIndex, "run");
        if (runtime.liveDebugReference == LUA_NOREF) {
            throw std::runtime_error(
                "Live debug is unavailable in the current scene");
        }
        lua_rawgeti(state, LUA_REGISTRYINDEX, runtime.liveDebugReference);
        lua_pushvalue(state, messageIndex);
        if (ludork::standard::protectedLuaCall(state, 1, LUA_MULTRET) !=
            LUA_OK) {
            const char* error = lua_tostring(state, -1);
            throw std::runtime_error(
                error == nullptr ? "Live debug request failed" : error);
        }
        if (lua_gettop(state) != stackBase + 1 || !lua_istable(state, -1)) {
            throw std::runtime_error(
                "Live debug handler must return exactly one response table");
        }
        const std::string payload = encodeValue(state, runtime, -1);
        lua_settop(state, stackBase);
        queueResponse(state, runtime, requestId, run, payload);
    } catch (const std::exception& error) {
        lua_settop(state, stackBase);
        try {
            queueFailure(state, runtime, requestId, run, error.what());
        } catch (const std::exception&) {
            lua_settop(state, stackBase);
            try {
                queueFailure(state, runtime, requestId, run,
                             "Live debug response could not be delivered");
            } catch (const std::exception&) {
                runtime.client.disconnect();
            }
        }
    }
    lua_settop(state, stackBase);
}

}  // namespace ludork::standard::runtime
