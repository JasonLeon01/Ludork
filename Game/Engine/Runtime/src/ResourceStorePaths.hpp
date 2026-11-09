#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ludork::runtime::detail {

[[nodiscard]] std::string asciiFold(std::string value);

[[nodiscard]] bool isLinkLike(const std::filesystem::path& path,
                              const std::filesystem::file_status& status,
                              std::string_view inspectionError);

[[nodiscard]] std::filesystem::path resourceStoreRoot(
    const std::filesystem::path& runtimeRoot, const std::string& name,
    bool packed);

[[nodiscard]] std::filesystem::path resourceUserDataRoot(
    const std::filesystem::path& root);

[[nodiscard]] std::optional<std::filesystem::directory_entry> findLooseResource(
    const std::filesystem::path& root, const std::filesystem::path& relative);

}  // namespace ludork::runtime::detail
