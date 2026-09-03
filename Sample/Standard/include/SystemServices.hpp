#pragma once

#include <StandardApi.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <tuple>
#include <vector>

namespace ludork::standard {

LUDORK_STANDARD_API double performanceCounter();
LUDORK_STANDARD_API void setDefaultLocale(const std::string& language);
LUDORK_STANDARD_API std::tuple<std::string, std::string> defaultLocale();
LUDORK_STANDARD_API std::filesystem::path currentWorkingDirectory();
LUDORK_STANDARD_API void createDirectories(const std::filesystem::path& value);
LUDORK_STANDARD_API void removeFile(const std::filesystem::path& value);
LUDORK_STANDARD_API std::vector<std::filesystem::path> listDirectory(
    const std::filesystem::path& value);
LUDORK_STANDARD_API std::filesystem::path joinPath(
    const std::vector<std::filesystem::path>& parts);
LUDORK_STANDARD_API std::tuple<std::filesystem::path, std::filesystem::path>
splitExtension(const std::filesystem::path& value);
LUDORK_STANDARD_API std::filesystem::path baseName(
    const std::filesystem::path& value);
LUDORK_STANDARD_API std::filesystem::path directoryName(
    const std::filesystem::path& value);
LUDORK_STANDARD_API std::filesystem::path absolutePath(
    const std::filesystem::path& value);
LUDORK_STANDARD_API bool isDirectory(const std::filesystem::path& value);
LUDORK_STANDARD_API bool isRegularFile(const std::filesystem::path& value);
LUDORK_STANDARD_API double modificationTime(const std::filesystem::path& value);
LUDORK_STANDARD_API double processMemoryMegabytes();

}  // namespace ludork::standard
