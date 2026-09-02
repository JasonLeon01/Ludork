#pragma once

#include <StandardApi.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ludork::standard {

LUDORK_STANDARD_API std::string encodeBase64(
    std::span<const std::uint8_t> bytes);

LUDORK_STANDARD_API std::vector<std::uint8_t> decodeBase64(
    std::string_view value);

}  // namespace ludork::standard
