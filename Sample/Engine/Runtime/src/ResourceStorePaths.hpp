#pragma once

#include <filesystem>
#include <string>

namespace ludork::runtime::detail {

[[nodiscard]] std::filesystem::path resourceStoreRoot(
    const std::filesystem::path& runtimeRoot, const std::string& name,
    bool packed);

}  // namespace ludork::runtime::detail
