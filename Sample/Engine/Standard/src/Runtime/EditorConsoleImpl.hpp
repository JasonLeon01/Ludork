#pragma once

#include <EditorCommandServices.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

extern "C" {
#include <lauxlib.h>
}

#include <string>
#include <unordered_map>

namespace ludork::standard::runtime {

struct EditorConsoleImpl {
    lua_State* state{};
    sf::TcpListener listener;
    sf::TcpSocket client;
    std::string input;
    int environmentReference{LUA_NOREF};
    int jsonDecodeReference{LUA_NOREF};
    int inputInjectReference{LUA_NOREF};
    int shutdownReference{LUA_NOREF};
    EditorCommandReloadHandler reloadHandler{};
    std::unordered_map<std::string, EditorCommandBoolControlHandler>
        boolControlHandlers;
    bool connected{};
};

}  // namespace ludork::standard::runtime
