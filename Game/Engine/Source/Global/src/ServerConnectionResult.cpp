#include <Server/ServerConnectionResult.hpp>

#include <utility>

ServerConnectionResult::ServerConnectionResult(
    ServerErrorCode code, std::string message,
    std::optional<double> roundTripMs)
    : ServerResult(code, std::move(message)),
      state_(code == ServerErrorCode::None && roundTripMs
                 ? (*roundTripMs <= 300 ? NetworkState::Normal
                                        : NetworkState::Weak)
                 : NetworkState::Disconnected),
      roundTripMs_(code == ServerErrorCode::None ? roundTripMs : std::nullopt) {
}

ServerConnectionResult::NetworkState ServerConnectionResult::getState() const {
    return state_;
}
std::optional<double> ServerConnectionResult::getRoundTripMs() const {
    return roundTripMs_;
}
