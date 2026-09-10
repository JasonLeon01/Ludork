#pragma once

#include <StandardApi.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

namespace ludork::standard {

enum class ReadOnlyFileType : std::uint8_t {
    Missing,
    Regular,
    Directory,
};

struct ReadOnlyFileStatus {
    bool handled = false;
    ReadOnlyFileType type = ReadOnlyFileType::Missing;
    std::uintmax_t size = 0;
    double modificationTime = 0.0;
};

struct ReadOnlyFileProvider {
    std::function<ReadOnlyFileStatus(const std::filesystem::path&)> status;
    std::function<std::vector<std::filesystem::path>(
        const std::filesystem::path&)>
        listDirectory;
    std::function<std::vector<std::uint8_t>(const std::filesystem::path&)>
        readFile;
};

LUDORK_STANDARD_API void configureReadOnlyFileProvider(
    ReadOnlyFileProvider provider);
LUDORK_STANDARD_API void clearReadOnlyFileProvider() noexcept;

[[nodiscard]] LUDORK_STANDARD_API ReadOnlyFileStatus
readOnlyFileStatus(const std::filesystem::path& path);
[[nodiscard]] LUDORK_STANDARD_API std::vector<std::filesystem::path>
readOnlyDirectoryEntries(const std::filesystem::path& path);
[[nodiscard]] LUDORK_STANDARD_API std::vector<std::uint8_t> readOnlyFileBytes(
    const std::filesystem::path& path);

}  // namespace ludork::standard
