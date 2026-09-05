#pragma once

#include <StandardApi.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace ludork::standard {

LUDORK_STANDARD_API std::vector<std::uint8_t> compressZlib(
    std::span<const std::uint8_t> bytes, int level);

LUDORK_STANDARD_API std::vector<std::uint8_t> decompressZlib(
    std::span<const std::uint8_t> bytes, int windowBits);

}  // namespace ludork::standard
