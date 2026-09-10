#pragma once

#include <StandardApi.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ludork::standard::unicode {

LUDORK_STANDARD_API void validateUtf8(std::string_view text);
LUDORK_STANDARD_API std::vector<std::size_t> graphemeOffsets(
    std::string_view text);
LUDORK_STANDARD_API std::size_t graphemeLength(std::string_view text);
LUDORK_STANDARD_API std::string stripWhitespace(std::string_view text);
LUDORK_STANDARD_API std::string sanitizeSingleLine(std::string_view text);

}  // namespace ludork::standard::unicode
