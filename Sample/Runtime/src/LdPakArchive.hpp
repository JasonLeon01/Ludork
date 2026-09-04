#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ludork::runtime::detail {

struct LdPakEntry {
    std::string path;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint32_t crc = 0;
    bool directory = false;
};

class LdPakArchive final {
public:
    explicit LdPakArchive(const std::filesystem::path& path);
    ~LdPakArchive();

    LdPakArchive(const LdPakArchive&) = delete;
    LdPakArchive& operator=(const LdPakArchive&) = delete;
    LdPakArchive(LdPakArchive&&) noexcept;
    LdPakArchive& operator=(LdPakArchive&&) noexcept;

    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] const std::string& group() const noexcept;
    [[nodiscard]] double modificationTime() const noexcept;
    [[nodiscard]] const std::vector<LdPakEntry>& entries() const noexcept;
    [[nodiscard]] std::vector<std::uint8_t> readAll(
        const std::string& relativePath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::uint32_t calculateLdPakDataCrc(
    const std::filesystem::path& path, std::uint64_t offset,
    std::uint64_t size);

}  // namespace ludork::runtime::detail
