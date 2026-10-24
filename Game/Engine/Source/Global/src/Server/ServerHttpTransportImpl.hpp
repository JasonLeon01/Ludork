#pragma once

#include "ServerRequestData.hpp"

#include <SFML/Network/TcpSocket.hpp>

#include <chrono>
#include <cstddef>
#include <string_view>

namespace ludork::global::server_impl {

class ServerHttpTransportImpl {
public:
    explicit ServerHttpTransportImpl(const PendingRequest& pending);
    Response perform();

private:
    static constexpr std::chrono::milliseconds ConnectionAttemptInterval{250};
    static constexpr std::chrono::milliseconds IoPollInterval{5};
    static constexpr int SelectorPollMilliseconds = 10;
    static constexpr std::size_t ReadBufferSize = 8192;

    struct Endpoint {
        std::string host;
        std::string authority;
        std::string prefix;
        std::string project;
        unsigned short port = 80;
    };

    struct Headers {
        int status = 0;
        std::optional<std::size_t> length;
        bool chunked = false;
    };

    static Endpoint parseEndpoint();
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
};

}  // namespace ludork::global::server_impl
