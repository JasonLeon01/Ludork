#pragma once

#include "ServerRequestData.hpp"

#if !defined(__EMSCRIPTEN__)
#include <SFML/Network/TcpSocket.hpp>
#endif

#include <chrono>
#include <cstddef>
#include <string_view>

namespace ludork::global::server_impl {

class ServerHttpTransportImpl {
public:
    struct Endpoint {
        std::string host;
        std::string authority;
        std::string prefix;
        std::string project;
        unsigned short port = 80;
    };

    static Endpoint parseEndpoint();
    static Response decodeResponse(const PendingRequest& pending,
                                   const std::string& project, int status,
                                   const std::string& body);

#if !defined(__EMSCRIPTEN__)
    explicit ServerHttpTransportImpl(const PendingRequest& pending);
    Response perform();

private:
    static constexpr std::chrono::milliseconds ConnectionAttemptInterval{250};
    static constexpr std::chrono::milliseconds IoPollInterval{5};
    static constexpr int SelectorPollMilliseconds = 10;
    static constexpr std::size_t ReadBufferSize = 8192;

    struct Headers {
        int status = 0;
        std::optional<std::size_t> length;
        bool chunked = false;
    };

    static Headers parseHeaders(std::string_view text);
    static std::optional<std::string> decodeChunked(std::string_view text);
    bool stopped() const;
    bool connect(const Endpoint& endpoint);
    bool send(const std::string& bytes);
    std::optional<std::string> receive();
    Response failure(ServerErrorCode code, const std::string& message) const;
    const PendingRequest& pending_;
    sf::TcpSocket socket_;
    int status_ = 0;
#endif
};

}  // namespace ludork::global::server_impl
