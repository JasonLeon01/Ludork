#include <Base64.hpp>

#include <array>
#include <cctype>
#include <stdexcept>

namespace ludork::standard {

std::string encodeBase64(std::span<const std::uint8_t> bytes) {
    static constexpr char Alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((bytes.size() + 2) / 3) * 4);
    for (std::size_t index = 0; index < bytes.size(); index += 3) {
        const std::uint32_t first = bytes[index];
        const std::uint32_t second =
            index + 1 < bytes.size() ? bytes[index + 1] : 0U;
        const std::uint32_t third =
            index + 2 < bytes.size() ? bytes[index + 2] : 0U;
        const std::uint32_t value = (first << 16U) | (second << 8U) | third;
        result.push_back(Alphabet[(value >> 18U) & 0x3FU]);
        result.push_back(Alphabet[(value >> 12U) & 0x3FU]);
        result.push_back(
            index + 1 < bytes.size() ? Alphabet[(value >> 6U) & 0x3FU] : '=');
        result.push_back(index + 2 < bytes.size() ? Alphabet[value & 0x3FU]
                                                  : '=');
    }
    return result;
}

std::vector<std::uint8_t> decodeBase64(std::string_view value) {
    static constexpr std::array<signed char, 256> Lookup = [] {
        std::array<signed char, 256> result{};
        result.fill(-1);
        constexpr std::string_view alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (std::size_t index = 0; index < alphabet.size(); ++index) {
            result[static_cast<unsigned char>(alphabet[index])] =
                static_cast<signed char>(index);
        }
        return result;
    }();

    std::vector<std::uint8_t> result;
    result.reserve(value.size() * 3 / 4);
    std::uint32_t buffer = 0;
    int bits = 0;
    std::size_t dataSize = 0;
    std::size_t paddingSize = 0;
    bool padding = false;
    for (const unsigned char character : value) {
        if (character == '=') {
            padding = true;
            ++paddingSize;
            if (paddingSize > 2) {
                throw std::invalid_argument("Invalid base64 data");
            }
            continue;
        }
        if (std::isspace(character) != 0) {
            continue;
        }
        if (padding) {
            throw std::invalid_argument("Invalid base64 data");
        }
        const int decoded = Lookup[character];
        if (decoded < 0) {
            throw std::invalid_argument("Invalid base64 data");
        }
        ++dataSize;
        buffer = (buffer << 6U) | static_cast<std::uint32_t>(decoded);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            result.push_back(
                static_cast<std::uint8_t>((buffer >> bits) & 0xFFU));
        }
    }
    if (dataSize % 4 == 1 ||
        (paddingSize != 0 && ((dataSize + paddingSize) % 4 != 0 ||
                              paddingSize != 4 - dataSize % 4)) ||
        (bits != 0 &&
         (buffer & ((static_cast<std::uint32_t>(1) << bits) - 1U)) != 0)) {
        throw std::invalid_argument("Invalid base64 data");
    }
    return result;
}

}  // namespace ludork::standard
