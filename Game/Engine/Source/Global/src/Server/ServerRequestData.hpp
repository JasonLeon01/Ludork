#pragma once

#include <Runtime/RuntimeData.hpp>
#include <Server/ServerErrorCode.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace ludork::global::server_impl {

struct Request {
    std::string method;
    std::string path;
    std::string bearer;
    std::string body;
    bool health = false;
};

struct Response {
    ServerErrorCode code = ServerErrorCode::None;
    std::string message;
    RuntimeData body;
    double roundTripMs = 0;
    std::optional<std::int64_t> retryAfterSeconds;
};

struct PendingRequest {
    using Clock = std::chrono::steady_clock;
    Request request;
    Clock::time_point started = Clock::now();
    Clock::time_point deadline = started + std::chrono::seconds(10);
    std::atomic_bool cancelled = false;
    std::mutex mutex;
    std::optional<Response> response;
};

std::string encodeSegment(const std::string& value);
bool validIdentifier(const std::string& value);

}  // namespace ludork::global::server_impl
