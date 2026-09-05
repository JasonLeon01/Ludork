#include "Protocol/PreviewProtocol.hpp"

#include <Runtime/Json.hpp>

#include <array>
#include <cstdio>
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace ludork::preview_host {
namespace {

constexpr std::uint32_t maximumMessageSize = 64u * 1024u * 1024u;

std::array<std::uint8_t, 4> littleEndian(std::uint32_t value) {
    return {
        static_cast<std::uint8_t>(value & 0xffu),
        static_cast<std::uint8_t>((value >> 8u) & 0xffu),
        static_cast<std::uint8_t>((value >> 16u) & 0xffu),
        static_cast<std::uint8_t>((value >> 24u) & 0xffu),
    };
}

}  // namespace

RuntimeData::Map object(
    std::initializer_list<std::pair<const std::string, RuntimeData>> values) {
    RuntimeData::Map result;
    for (const auto& [name, value] : values) {
        result.emplace(name, value);
    }
    return result;
}

RuntimeData number(float value) {
    return RuntimeData(static_cast<double>(value));
}

RuntimeData errorResponse(const std::string& message) {
    return RuntimeData(object({
        {"type", RuntimeData("error")},
        {"message", RuntimeData(message)},
    }));
}

void configureProtocolStreams() {
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        throw std::runtime_error(
            "Failed to set preview protocol stdin to binary mode");
    }
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        throw std::runtime_error(
            "Failed to set preview protocol stdout to binary mode");
    }
#endif
}

std::optional<std::string> readMessage() {
    std::array<std::uint8_t, 4> lengthBytes{};
    std::cin.read(reinterpret_cast<char*>(lengthBytes.data()),
                  static_cast<std::streamsize>(lengthBytes.size()));
    if (std::cin.gcount() == 0 && std::cin.eof()) {
        return std::nullopt;
    }
    if (std::cin.gcount() != static_cast<std::streamsize>(lengthBytes.size())) {
        throw std::runtime_error("Truncated preview protocol length");
    }
    const std::uint32_t length =
        static_cast<std::uint32_t>(lengthBytes[0]) |
        (static_cast<std::uint32_t>(lengthBytes[1]) << 8u) |
        (static_cast<std::uint32_t>(lengthBytes[2]) << 16u) |
        (static_cast<std::uint32_t>(lengthBytes[3]) << 24u);
    if (length == 0 || length > maximumMessageSize) {
        throw std::runtime_error("Invalid preview protocol message length");
    }
    std::string message(length, '\0');
    std::cin.read(message.data(), static_cast<std::streamsize>(length));
    if (std::cin.gcount() != static_cast<std::streamsize>(length)) {
        throw std::runtime_error("Truncated preview protocol message");
    }
    return message;
}

void writeMessage(const RuntimeData& value) {
    const std::string message = stringifyJSON(value);
    if (message.empty() || message.size() > maximumMessageSize) {
        throw std::runtime_error(
            "Preview protocol response has an invalid size");
    }
    const auto length =
        littleEndian(static_cast<std::uint32_t>(message.size()));
    std::cout.write(reinterpret_cast<const char*>(length.data()),
                    static_cast<std::streamsize>(length.size()));
    std::cout.write(message.data(),
                    static_cast<std::streamsize>(message.size()));
    std::cout.flush();
    if (!std::cout) {
        throw std::runtime_error("Failed to write preview protocol response");
    }
}

}  // namespace ludork::preview_host
