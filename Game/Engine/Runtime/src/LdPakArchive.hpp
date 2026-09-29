#pragma once

#include "LdPakEntry.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ludork::runtime::detail {

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
    [[nodiscard]] const LdPakEntry& entry(
        const std::string& relativePath) const;
    [[nodiscard]] std::vector<std::uint8_t> readAll(
        const std::string& relativePath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::runtime::detail
