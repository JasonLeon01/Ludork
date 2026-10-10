#include <LudorkGenerated/ServerConfig.hpp>

#if defined(LUDORK_SERVER_AVAILABLE)
#include "ServerHttpTransportImpl.hpp"

#include <Runtime/Json.hpp>
#include <SFML/Network/Dns.hpp>
#include <SFML/Network/SocketSelector.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cctype>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ludork::global::server_impl {
namespace {
constexpr std::size_t MaxHeaderSize = 16 * 1024;
constexpr std::size_t MaxBodySize = 2 * 1024 * 1024;

std::string lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char byte) {
                       return static_cast<char>(std::tolower(byte));
                   });
    return result;
}

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

std::size_t number(std::string_view value, int base = 10) {
    std::size_t result = 0;
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result, base);
    if (value.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size()) {
        throw std::runtime_error("Invalid HTTP number.");
    }
    return result;
}

ServerErrorCode errorCode(const std::string& code) {
    static const std::unordered_map<std::string, ServerErrorCode> codes{
        {"InvalidArgument", ServerErrorCode::InvalidArgument},
        {"AuthenticationFailed", ServerErrorCode::AuthenticationFailed},
        {"InvalidWriteToken", ServerErrorCode::InvalidWriteToken},
        {"Timeout", ServerErrorCode::Timeout},
        {"ConnectionFailed", ServerErrorCode::ConnectionFailed},
        {"InvalidResponse", ServerErrorCode::InvalidResponse},
        {"StorageFailure", ServerErrorCode::StorageFailure},
        {"ServerFailure", ServerErrorCode::ServerFailure},
        {"RateLimited", ServerErrorCode::RateLimited}};
    const auto found = codes.find(code);
    return found == codes.end() ? ServerErrorCode::ServerFailure
                                : found->second;
}
}  // namespace

ServerHttpTransportImpl::ServerHttpTransportImpl(const PendingRequest& pending)
    : pending_(pending) {
    socket_.setBlocking(false);
}

ServerHttpTransportImpl::Endpoint ServerHttpTransportImpl::parseEndpoint() {
    const std::string url = ludork::generated::server::Url;
    if (!url.starts_with("http://") ||
        url.find_first_of("?#@\\\r\n\t ") != std::string::npos) {
        throw std::invalid_argument("Invalid server URL.");
    }
    const std::size_t slash = url.find('/', 7);
    if (slash == std::string::npos) {
        throw std::invalid_argument("Missing server project path.");
    }
    Endpoint result;
    result.authority = url.substr(7, slash - 7);
    result.prefix = url.substr(slash);
    while (result.prefix.ends_with('/')) {
        result.prefix.pop_back();
    }
    if (result.prefix.size() < 2) {
        throw std::invalid_argument("Missing server project path.");
    }
    const std::string_view project(result.prefix.data() + 1,
                                   result.prefix.size() - 1);
    for (std::size_t index = 0; index < project.size(); ++index) {
        unsigned int byte = static_cast<unsigned char>(project[index]);
        if (byte == '%') {
            if (index + 2 >= project.size()) {
                throw std::invalid_argument("Invalid encoded project path.");
            }
            const auto parsed =
                std::from_chars(project.data() + index + 1,
                                project.data() + index + 3, byte, 16);
            if (parsed.ec != std::errc{} ||
                parsed.ptr != project.data() + index + 3) {
                throw std::invalid_argument("Invalid encoded project path.");
            }
            index += 2;
        }
        result.project.push_back(static_cast<char>(byte));
    }
    if (!validIdentifier(result.project) ||
        result.project.find_first_of("/\\") != std::string::npos) {
        throw std::invalid_argument("Invalid server project path.");
    }
    std::string_view authority(result.authority);
    std::string_view port;
    if (authority.starts_with('[')) {
        const std::size_t end = authority.find(']');
        if (end == std::string_view::npos) {
            throw std::invalid_argument("Invalid IPv6 server address.");
        }
        result.host = authority.substr(1, end - 1);
        const auto remainder = authority.substr(end + 1);
        if (!remainder.empty()) {
            if (!remainder.starts_with(':')) {
                throw std::invalid_argument("Invalid server authority.");
            }
            port = remainder.substr(1);
        }
    } else {
        const std::size_t colon = authority.find(':');
        result.host = authority.substr(0, colon);
        if (colon != std::string_view::npos) {
            port = authority.substr(colon + 1);
        }
    }
    if (result.host.empty() || authority.ends_with(':')) {
        throw std::invalid_argument("Invalid server host.");
    }
    if (!port.empty()) {
        unsigned int value = 0;
        const auto parsed =
            std::from_chars(port.data(), port.data() + port.size(), value);
        if (parsed.ec != std::errc{} ||
            parsed.ptr != port.data() + port.size() || value == 0 ||
            value > 65535) {
            throw std::invalid_argument("Invalid server port.");
        }
        result.port = static_cast<unsigned short>(value);
    }
    return result;
}

ServerHttpTransportImpl::Headers ServerHttpTransportImpl::parseHeaders(
    std::string_view text) {
    Headers result;
    const auto firstEnd = text.find("\r\n");
    const auto first = text.substr(0, firstEnd);
    if ((!first.starts_with("HTTP/1.0 ") && !first.starts_with("HTTP/1.1 ")) ||
        first.size() < 12 || (first.size() > 12 && first[12] != ' ')) {
        throw std::runtime_error("Invalid HTTP status line.");
    }
    result.status = static_cast<int>(number(first.substr(9, 3)));
    if (result.status < 100 || result.status > 599) {
        throw std::runtime_error("Invalid HTTP status.");
    }
    if (firstEnd == std::string_view::npos) {
        return result;
    }
    text.remove_prefix(firstEnd + 2);
    bool transferSeen = false;
    while (!text.empty()) {
        const auto end = text.find("\r\n");
        const auto line = text.substr(0, end);
        const auto colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            throw std::runtime_error("Invalid HTTP header.");
        }
        const auto name = lower(line.substr(0, colon));
        const auto value = trim(line.substr(colon + 1));
        if (name == "content-length") {
            const auto length = number(value);
            if (length > MaxBodySize || result.length.has_value()) {
                throw std::runtime_error("Invalid HTTP content length.");
            }
            result.length = length;
        } else if (name == "transfer-encoding") {
            if (transferSeen || lower(value) != "chunked") {
                throw std::runtime_error("Unsupported HTTP transfer encoding.");
            }
            transferSeen = true;
            result.chunked = true;
        } else if (name == "retry-after") {
            const auto seconds = number(value);
            if (result.retryAfterSeconds || seconds == 0 ||
                seconds > 9007199254740991ULL) {
                throw std::runtime_error("Invalid Retry-After header.");
            }
            result.retryAfterSeconds = static_cast<std::int64_t>(seconds);
        } else if (name == "content-encoding" && lower(value) != "identity") {
            throw std::runtime_error("Unsupported HTTP content encoding.");
        }
        if (end == std::string_view::npos) {
            break;
        }
        text.remove_prefix(end + 2);
    }
    if (result.length && result.chunked) {
        throw std::runtime_error("Ambiguous HTTP response framing.");
    }
    return result;
}

std::optional<std::string> ServerHttpTransportImpl::decodeChunked(
    std::string_view text) {
    std::string decoded;
    for (;;) {
        const auto lineEnd = text.find("\r\n");
        if (lineEnd == std::string_view::npos) {
            return {};
        }
        if (lineEnd > MaxHeaderSize) {
            throw std::runtime_error("Invalid HTTP chunk header.");
        }
        auto sizeText = text.substr(0, lineEnd);
        sizeText = sizeText.substr(0, sizeText.find(';'));
        const auto size = number(sizeText, 16);
        text.remove_prefix(lineEnd + 2);
        if (size > MaxBodySize - decoded.size()) {
            throw std::runtime_error("HTTP response is too large.");
        }
        if (size == 0) {
            if (text.starts_with("\r\n") ||
                text.find("\r\n\r\n") != std::string_view::npos) {
                return decoded;
            }
            return {};
        }
        if (text.size() < size + 2) {
            return {};
        }
        if (text.substr(size, 2) != "\r\n") {
            throw std::runtime_error("Invalid HTTP chunk ending.");
        }
        decoded.append(text.substr(0, size));
        text.remove_prefix(size + 2);
    }
}

bool ServerHttpTransportImpl::stopped() const {
    return pending_.cancelled.load() ||
           PendingRequest::Clock::now() >= pending_.deadline;
}

bool ServerHttpTransportImpl::connect(const Endpoint& endpoint) {
    if (stopped()) {
        return false;
    }
    const auto addresses = sf::Dns::resolve(endpoint.host);
    if (!addresses || stopped()) {
        return false;
    }
    sf::SocketSelector selector;
    std::vector<std::unique_ptr<sf::TcpSocket>> candidates;
    std::size_t nextAddress = 0;
    auto nextAttempt = PendingRequest::Clock::now();
    while (!stopped() &&
           (nextAddress < addresses->size() || !candidates.empty())) {
        if (nextAddress < addresses->size() &&
            (candidates.empty() ||
             PendingRequest::Clock::now() >= nextAttempt)) {
            auto candidate = std::make_unique<sf::TcpSocket>();
            candidate->setBlocking(false);
            const auto status =
                candidate->connect((*addresses)[nextAddress++], endpoint.port);
            nextAttempt =
                PendingRequest::Clock::now() + std::chrono::milliseconds(250);
            if (status == sf::Socket::Status::Done) {
                selector.clear();
                socket_ = std::move(*candidate);
                return true;
            }
            if (status == sf::Socket::Status::NotReady &&
                selector.add(*candidate, sf::SocketSelector::Send)) {
                candidates.push_back(std::move(candidate));
            }
        }
        if (candidates.empty() || !selector.wait(sf::milliseconds(10))) {
            continue;
        }
        for (auto candidate = candidates.begin();
             candidate != candidates.end();) {
            if (!selector.isReady(**candidate, sf::SocketSelector::Send)) {
                ++candidate;
                continue;
            }
            if ((*candidate)->getRemoteAddress()) {
                selector.clear();
                socket_ = std::move(**candidate);
                return true;
            }
            selector.remove(**candidate);
            candidate = candidates.erase(candidate);
        }
    }
    selector.clear();
    return false;
}

bool ServerHttpTransportImpl::send(const std::string& bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size() && !stopped()) {
        std::size_t sent = 0;
        const auto status =
            socket_.send(bytes.data() + offset, bytes.size() - offset, sent);
        offset += sent;
        if (status == sf::Socket::Status::Error ||
            status == sf::Socket::Status::Disconnected) {
            return false;
        }
        if (offset < bytes.size()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    return offset == bytes.size();
}

std::optional<std::string> ServerHttpTransportImpl::receive() {
    std::string bytes;
    std::optional<Headers> headers;
    std::size_t bodyStart = 0;
    std::array<char, 8192> buffer{};
    while (!stopped()) {
        std::size_t received = 0;
        const auto status =
            socket_.receive(buffer.data(), buffer.size(), received);
        if (status == sf::Socket::Status::Error) {
            return {};
        }
        bytes.append(buffer.data(), received);
        if (bytes.size() > MaxBodySize + 2 * MaxHeaderSize) {
            throw std::runtime_error("HTTP response is too large.");
        }
        if (!headers) {
            const auto end = bytes.find("\r\n\r\n");
            if (end != std::string::npos) {
                if (end > MaxHeaderSize) {
                    throw std::runtime_error("HTTP headers are too large.");
                }
                headers = parseHeaders(std::string_view(bytes).substr(0, end));
                status_ = headers->status;
                retryAfterSeconds_ = headers->retryAfterSeconds;
                bodyStart = end + 4;
            } else if (bytes.size() > MaxHeaderSize) {
                throw std::runtime_error("HTTP headers are too large.");
            }
        }
        if (headers) {
            const auto body = std::string_view(bytes).substr(bodyStart);
            if (headers->length && body.size() >= *headers->length) {
                return std::string(body.substr(0, *headers->length));
            }
            if (headers->chunked) {
                if (auto decoded = decodeChunked(body)) {
                    return decoded;
                }
            } else if (!headers->length && body.size() > MaxBodySize) {
                throw std::runtime_error("HTTP response is too large.");
            }
            if (status == sf::Socket::Status::Disconnected) {
                if (!headers->chunked && !headers->length) {
                    return std::string(body);
                }
                throw std::runtime_error("Incomplete HTTP response.");
            }
        } else if (status == sf::Socket::Status::Disconnected) {
            throw std::runtime_error("Missing HTTP response headers.");
        }
        if (received == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    return {};
}

Response ServerHttpTransportImpl::failure(ServerErrorCode code,
                                          const std::string& message) const {
    if (PendingRequest::Clock::now() >= pending_.deadline) {
        code = ServerErrorCode::Timeout;
    }
    return Response{code, code == ServerErrorCode::Timeout
                              ? "The server request timed out."
                              : message};
}

Response ServerHttpTransportImpl::perform() {
    const auto endpoint = parseEndpoint();
    if (!connect(endpoint)) {
        return failure(ServerErrorCode::ConnectionFailed,
                       "Could not connect to the server.");
    }
    const auto& request = pending_.request;
    std::string bytes = request.method + " " + endpoint.prefix + "/api/v1" +
                        request.path +
                        " HTTP/1.1\r\nHost: " + endpoint.authority +
                        "\r\nConnection: close\r\nAccept: "
                        "application/json\r\nAccept-Encoding: identity\r\n";
    if (!request.bearer.empty()) {
        bytes += "Authorization: Bearer " + request.bearer + "\r\n";
    } else if (!request.health) {
        bytes +=
            "X-Ludork-Key: " + std::string(ludork::generated::server::Key) +
            "\r\n";
    }
    bytes += "Content-Type: application/json\r\nContent-Length: " +
             std::to_string(request.body.size()) + "\r\n\r\n" + request.body;
    if (!send(bytes)) {
        return failure(ServerErrorCode::ConnectionFailed,
                       "Could not send the server request.");
    }
    try {
        const auto body = receive();
        if (!body) {
            return failure(ServerErrorCode::ConnectionFailed,
                           "Could not receive the server response.");
        }
        Response response;
        response.body = parseJSONText(*body);
        response.roundTripMs =
            std::chrono::duration<double, std::milli>(
                PendingRequest::Clock::now() - pending_.started)
                .count();
        const auto* object = response.body.getIf<RuntimeData::Map>();
        if (object == nullptr) {
            throw std::runtime_error("Expected a JSON response object.");
        }
        if (status_ != 200) {
            const auto code = object->find("code");
            const auto message = object->find("message");
            if (code == object->end() || message == object->end() ||
                code->second.getIf<std::string>() == nullptr ||
                message->second.getIf<std::string>() == nullptr) {
                throw std::runtime_error("Invalid server error response.");
            }
            response.code = errorCode(*code->second.getIf<std::string>());
            response.message = *message->second.getIf<std::string>();
            if (status_ == 429 ||
                response.code == ServerErrorCode::RateLimited) {
                if (status_ != 429 ||
                    response.code != ServerErrorCode::RateLimited ||
                    request.method != "PUT" ||
                    !object->contains("retryAfterSeconds")) {
                    throw std::runtime_error(
                        "Invalid write rate-limit response.");
                }
                const auto& retry = object->at("retryAfterSeconds");
                const auto* integer = retry.getIf<std::int64_t>();
                const auto* decimal = retry.getIf<double>();
                if (integer && *integer > 0 && *integer <= 9007199254740991LL) {
                    response.retryAfterSeconds = *integer;
                } else if (decimal && std::isfinite(*decimal) &&
                           *decimal >= 1 && *decimal <= 9007199254740991.0 &&
                           std::floor(*decimal) == *decimal) {
                    response.retryAfterSeconds =
                        static_cast<std::int64_t>(*decimal);
                } else {
                    throw std::runtime_error("Invalid write retry interval.");
                }
                if (response.retryAfterSeconds != retryAfterSeconds_) {
                    throw std::runtime_error(
                        "Inconsistent write retry interval.");
                }
            } else if (object->contains("retryAfterSeconds") ||
                       retryAfterSeconds_) {
                throw std::runtime_error("Unexpected write retry interval.");
            }
        } else if (request.health) {
            const auto service = object->find("service");
            const auto project = object->find("project");
            if (service == object->end() || project == object->end() ||
                service->second.getIf<std::string>() == nullptr ||
                *service->second.getIf<std::string>() != "LudorkServer" ||
                project->second.getIf<std::string>() == nullptr ||
                *project->second.getIf<std::string>() != endpoint.project) {
                throw std::runtime_error("Unexpected health response.");
            }
        }
        if (stopped()) {
            return failure(ServerErrorCode::Timeout,
                           "The server request timed out.");
        }
        return response;
    } catch (const std::exception&) {
        return failure(ServerErrorCode::InvalidResponse,
                       "The server returned an invalid response.");
    }
}

}  // namespace ludork::global::server_impl
#endif
