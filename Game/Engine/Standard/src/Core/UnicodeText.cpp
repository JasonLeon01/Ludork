#include <UnicodeText.hpp>

#include <utf8proc.h>

#include <stdexcept>

namespace ludork::standard::unicode {
namespace {

std::size_t decode(std::string_view text, std::size_t offset,
                   utf8proc_int32_t& codepoint) {
    const utf8proc_ssize_t count = utf8proc_iterate(
        reinterpret_cast<const utf8proc_uint8_t*>(text.data() + offset),
        static_cast<utf8proc_ssize_t>(text.size() - offset), &codepoint);
    if (count <= 0) {
        throw std::invalid_argument("Text must contain valid UTF-8");
    }
    return static_cast<std::size_t>(count);
}

bool whitespace(utf8proc_int32_t codepoint) {
    return (codepoint >= 0x09 && codepoint <= 0x0D) || codepoint == 0x20 ||
           codepoint == 0x85 || codepoint == 0xA0 || codepoint == 0x1680 ||
           (codepoint >= 0x2000 && codepoint <= 0x200A) ||
           codepoint == 0x2028 || codepoint == 0x2029 || codepoint == 0x202F ||
           codepoint == 0x205F || codepoint == 0x3000;
}

}  // namespace

void validateUtf8(std::string_view text) {
    std::size_t offset = 0;
    utf8proc_int32_t codepoint = 0;
    while (offset < text.size()) {
        offset += decode(text, offset, codepoint);
    }
}

std::vector<std::size_t> graphemeOffsets(std::string_view text) {
    std::vector<std::size_t> offsets{0};
    std::size_t offset = 0;
    utf8proc_int32_t previous = 0;
    utf8proc_int32_t state = 0;
    while (offset < text.size()) {
        utf8proc_int32_t codepoint = 0;
        const std::size_t length = decode(text, offset, codepoint);
        if (offset != 0 &&
            utf8proc_grapheme_break_stateful(previous, codepoint, &state)) {
            offsets.push_back(offset);
        }
        previous = codepoint;
        offset += length;
    }
    if (!text.empty()) {
        offsets.push_back(text.size());
    }
    return offsets;
}

std::size_t graphemeLength(std::string_view text) {
    return graphemeOffsets(text).size() - 1;
}

std::string stripWhitespace(std::string_view text) {
    std::size_t start = text.size();
    std::size_t end = 0;
    std::size_t offset = 0;
    while (offset < text.size()) {
        utf8proc_int32_t codepoint = 0;
        const std::size_t length = decode(text, offset, codepoint);
        if (!whitespace(codepoint)) {
            if (start == text.size()) {
                start = offset;
            }
            end = offset + length;
        }
        offset += length;
    }
    return end == 0 ? std::string{}
                    : std::string(text.substr(start, end - start));
}

std::string sanitizeSingleLine(std::string_view text) {
    std::string result;
    std::size_t offset = 0;
    bool previousCarriageReturn = false;
    while (offset < text.size()) {
        utf8proc_int32_t codepoint = 0;
        const std::size_t length = decode(text, offset, codepoint);
        if (codepoint == 0x09 || codepoint == 0x0A || codepoint == 0x0D ||
            codepoint == 0x85 || codepoint == 0x2028 || codepoint == 0x2029) {
            if (codepoint != 0x0A || !previousCarriageReturn) {
                result.push_back(' ');
            }
        } else if (utf8proc_category(codepoint) != UTF8PROC_CATEGORY_CC) {
            result.append(text.substr(offset, length));
        }
        previousCarriageReturn = codepoint == 0x0D;
        offset += length;
    }
    return result;
}

}  // namespace ludork::standard::unicode
