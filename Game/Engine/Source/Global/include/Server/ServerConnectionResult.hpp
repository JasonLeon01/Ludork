#pragma once

#include <Server/ServerResult.hpp>

#include <optional>

BIND_CLASS()
class LUDORK_GLOBAL_API ServerConnectionResult : public ServerResult {
public:
    LUDORK_CAST_DERIVED(ServerConnectionResult, ServerResult)

    BIND_ENUM(name = "ServerNetworkState")
    enum class NetworkState {
        Normal,
        Weak,
        Disconnected
    };

    ServerConnectionResult(ServerErrorCode code, std::string message,
                           std::optional<double> roundTripMs = {});

    BIND_METHOD(property = "state")
    NetworkState getState() const;
    BIND_METHOD(property = "roundTripMs")
    std::optional<double> getRoundTripMs() const;

private:
    NetworkState state_;
    std::optional<double> roundTripMs_;
};
