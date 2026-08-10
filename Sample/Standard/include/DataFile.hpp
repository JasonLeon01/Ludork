#pragma once

#include <StandardApi.hpp>

#include <filesystem>
#include <string>

namespace ludork::standard {

LUDORK_STANDARD_API std::filesystem::path resolveJsonDataPath(
    const std::filesystem::path& path);

LUDORK_STANDARD_API std::filesystem::path logicalJsonDataPath(
    const std::filesystem::path& path);

LUDORK_STANDARD_API bool jsonDataExists(const std::filesystem::path& path);

LUDORK_STANDARD_API std::string readJsonText(const std::filesystem::path& path);

LUDORK_STANDARD_API void writeJsonText(const std::filesystem::path& path,
                                       const std::string& source);

}  // namespace ludork::standard
