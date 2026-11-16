#pragma once

#include <LudorkGenerated/ServerConfig.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#if defined(LUDORK_SERVER_AVAILABLE)
#if defined(__EMSCRIPTEN__)
#include <emscripten/websocket.h>
#else
#include <ixwebsocket/IXWebSocket.h>
#endif
#endif

namespace ludork::global::net_impl {

class NetSocketImpl {
public:
    enum class State {
        Connecting,
        Open,
        Closing,
        Closed
    };
    struct Notification {
        enum class Kind {
            Open,
            Message,
            Close,
            Error,
            Overflow
        };
        Kind kind = Kind::Message;
        std::uint64_t generation = 0;
        std::string bytes;
        bool binary = false;
        int status = 0;
    };

    NetSocketImpl(std::string url, std::uint64_t generation,
                  std::size_t maximumMessageBytes,
                  std::function<void(Notification)> callback);
    ~NetSocketImpl();
    void start();
    void stop();
    State getReadyState() const;
    std::size_t bufferedAmount() const;
    bool sendText(const std::string& text);
    void close(unsigned short code = 1000, const char* reason = "");

private:
#if defined(LUDORK_SERVER_AVAILABLE) && defined(__EMSCRIPTEN__)
    static bool onOpen(int, const EmscriptenWebSocketOpenEvent*, void*);
    static bool onMessage(int, const EmscriptenWebSocketMessageEvent*, void*);
    static bool onClose(int, const EmscriptenWebSocketCloseEvent*, void*);
    static bool onError(int, const EmscriptenWebSocketErrorEvent*, void*);
    EMSCRIPTEN_WEBSOCKET_T socket_ = 0;
#elif defined(LUDORK_SERVER_AVAILABLE)
    std::unique_ptr<ix::WebSocket> socket_;
#endif
    std::string url_;
    std::uint64_t generation_;
    std::size_t maximumMessageBytes_;
    std::function<void(Notification)> callback_;
};

}  // namespace ludork::global::net_impl
