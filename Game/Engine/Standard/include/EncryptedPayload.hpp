#pragma once

#include <StandardApi.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ludork::standard {

struct EncryptedPayloadFormat {
    std::array<std::uint8_t, 4> magic;
    std::uint32_t maximumSourceSize;
    std::string_view formatName;
};

LUDORK_STANDARD_API std::vector<std::uint8_t> encodeEncryptedPayload(
    std::string_view source, std::uint64_t nonce,
    const EncryptedPayloadFormat& format, std::string_view context,
    std::string_view sourceName);

LUDORK_STANDARD_API std::string decodeEncryptedPayload(
    std::span<const std::uint8_t> encoded, const EncryptedPayloadFormat& format,
    std::string_view context);

}  // namespace ludork::standard
