#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ludork::standard::detail {

struct Utf8Codepoint {
    std::size_t offset;
    std::size_t length;
    char32_t value;
};

[[noreturn]] inline void throwInvalidUtf8(std::string_view name,
                                          std::size_t offset) {
    throw std::invalid_argument(std::string(name) +
                                " contains invalid UTF-8 at byte " +
                                std::to_string(offset + 1));
}

inline Utf8Codepoint decodeUtf8Codepoint(std::string_view value,
                                         std::string_view name,
                                         std::size_t offset) {
    const std::uint8_t lead =
        static_cast<std::uint8_t>(static_cast<unsigned char>(value[offset]));
    std::size_t length = 0;
    char32_t codepoint = 0;
    if (lead <= 0x7F) {
        length = 1;
        codepoint = lead;
    } else if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
        codepoint = lead & 0x1F;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        length = 3;
        codepoint = lead & 0x0F;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        length = 4;
        codepoint = lead & 0x07;
    } else {
        throwInvalidUtf8(name, offset);
    }
    if (offset + length > value.size()) {
        throwInvalidUtf8(name, offset);
    }
    for (std::size_t index = 1; index < length; ++index) {
        const std::uint8_t continuation = static_cast<std::uint8_t>(
            static_cast<unsigned char>(value[offset + index]));
        if ((continuation & 0xC0) != 0x80) {
            throwInvalidUtf8(name, offset + index);
        }
        codepoint =
            (codepoint << 6) | static_cast<char32_t>(continuation & 0x3F);
    }
    if ((lead == 0xE0 &&
         static_cast<unsigned char>(value[offset + 1]) < 0xA0) ||
        (lead == 0xED &&
         static_cast<unsigned char>(value[offset + 1]) > 0x9F) ||
        (lead == 0xF0 &&
         static_cast<unsigned char>(value[offset + 1]) < 0x90) ||
        (lead == 0xF4 &&
         static_cast<unsigned char>(value[offset + 1]) > 0x8F)) {
        throwInvalidUtf8(name, offset);
    }
    return {offset, length, codepoint};
}

inline void validateUtf8(std::string_view value, std::string_view name) {
    std::size_t offset = 0;
    while (offset < value.size()) {
        offset += decodeUtf8Codepoint(value, name, offset).length;
    }
}

}  // namespace ludork::standard::detail
