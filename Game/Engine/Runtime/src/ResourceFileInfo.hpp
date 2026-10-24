#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace ludork::runtime::detail {

[[nodiscard]] double resourceModificationTime(const std::filesystem::path& path,
                                              std::string_view failurePrefix);

[[nodiscard]] std::uint64_t resourceFileSize(const std::filesystem::path& path,
                                             std::string_view failurePrefix,
                                             std::string_view overflowMessage);

}  // namespace ludork::runtime::detail
