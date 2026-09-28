#pragma once

#include <filesystem>
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

}  // namespace ludork::runtime::detail
