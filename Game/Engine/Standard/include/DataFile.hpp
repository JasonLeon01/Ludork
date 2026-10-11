#pragma once

#include <StandardApi.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace ludork::standard {

LUDORK_STANDARD_API std::filesystem::path resolveDataPath(
    const std::filesystem::path& path);

LUDORK_STANDARD_API std::filesystem::path logicalDataPath(
    const std::filesystem::path& path);

LUDORK_STANDARD_API bool dataFileExists(const std::filesystem::path& path);

LUDORK_STANDARD_API bool fileExists(const std::filesystem::path& path);
LUDORK_STANDARD_API std::string readFileBytes(
    const std::filesystem::path& path);
LUDORK_STANDARD_API void writeFileBytes(const std::filesystem::path& path,
                                        std::string_view bytes);
LUDORK_STANDARD_API bool isLdcData(std::string_view bytes);
LUDORK_STANDARD_API std::string decodeDataPayload(std::string_view bytes,
                                                  std::string_view context);
LUDORK_STANDARD_API std::string encodeDataPayload(
    const std::filesystem::path& path, const std::string& payload);

}  // namespace ludork::standard
