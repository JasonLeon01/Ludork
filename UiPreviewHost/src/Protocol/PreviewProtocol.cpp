#include "Protocol/PreviewProtocol.hpp"

#include <Utils/File.hpp>

#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
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

const RuntimeValue* findValue(const RuntimeValue::Map& values,
                              const std::string& name) {
    const auto iterator = values.find(name);
    return iterator == values.end() ? nullptr : &iterator->second;
}

const RuntimeValue::Map& requireMap(const RuntimeValue& value,
                                    const std::string& source) {
    const RuntimeValue::Map* map = value.getIf<RuntimeValue::Map>();
    if (map == nullptr) {
        throw std::invalid_argument(source + " must be an object");
    }
    return *map;
}

const RuntimeValue::Array& requireArray(const RuntimeValue& value,
                                        const std::string& source) {
    const RuntimeValue::Array* array = value.getIf<RuntimeValue::Array>();
    if (array == nullptr) {
        throw std::invalid_argument(source + " must be an array");
    }
    return *array;
}

const RuntimeValue& requireValue(const RuntimeValue::Map& values,
                                 const std::string& name,
                                 const std::string& source) {
    const RuntimeValue* value = findValue(values, name);
    if (value == nullptr) {
        throw std::invalid_argument(source + " is missing " + name);
    }
    return *value;
}

const std::string& requireString(const RuntimeValue& value,
                                 const std::string& source) {
    const std::string* text = value.getIf<std::string>();
    if (text == nullptr) {
        throw std::invalid_argument(source + " must be a string");
    }
    return *text;
}

std::int64_t requireInteger(const RuntimeValue& value,
                            const std::string& source) {
    const std::int64_t* integer = value.getIf<std::int64_t>();
    if (integer == nullptr) {
        throw std::invalid_argument(source + " must be an integer");
    }
    return *integer;
}

double requireNumber(const RuntimeValue& value, const std::string& source) {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return static_cast<double>(*integer);
    }
    const double* number = value.getIf<double>();
    if (number == nullptr || !std::isfinite(*number)) {
        throw std::invalid_argument(source + " must be a finite number");
    }
    return *number;
}

int requireInt32(const RuntimeValue& value, const std::string& source) {
    const std::int64_t integer = requireInteger(value, source);
    if (integer < std::numeric_limits<int>::min() ||
        integer > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(source + " is outside the integer range");
    }
    return static_cast<int>(integer);
}

RuntimeValue::Map object(
    std::initializer_list<std::pair<const std::string, RuntimeValue>> values) {
    RuntimeValue::Map result;
    for (const auto& [name, value] : values) {
        result.emplace(name, value);
    }
    return result;
}

RuntimeValue number(float value) {
    return RuntimeValue(static_cast<double>(value));
}

RuntimeValue errorResponse(const std::string& message) {
    return RuntimeValue(object({
        {"type", RuntimeValue("error")},
        {"message", RuntimeValue(message)},
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

void writeMessage(const RuntimeValue& value) {
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
