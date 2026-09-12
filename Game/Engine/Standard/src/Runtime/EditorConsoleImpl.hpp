#pragma once

#include <EditorCommandServices.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

extern "C" {
#include <lauxlib.h>
}

#include <string>
#include <deque>
#include <unordered_map>

namespace ludork::standard::runtime {

struct EditorConsoleImpl {
    lua_State* state{};
    sf::TcpListener listener;
    sf::TcpSocket client;
    std::string input;
    std::deque<std::string> output;
    std::deque<std::string> liveDebugOutput;
    std::size_t liveDebugOutputSize{};
    std::size_t outputOffset{};
    std::size_t outputSize{};
    std::uint64_t connectionId{};
    int environmentReference{LUA_NOREF};
    int jsonDecodeReference{LUA_NOREF};
    int jsonEncodeReference{LUA_NOREF};
    int liveDebugReference{LUA_NOREF};
    int inputInjectReference{LUA_NOREF};
    int shutdownReference{LUA_NOREF};
    EditorCommandReloadHandler reloadHandler{};
    std::unordered_map<std::string, EditorCommandBoolControlHandler>
        boolControlHandlers;
    bool connected{};
};

}  // namespace ludork::standard::runtime
