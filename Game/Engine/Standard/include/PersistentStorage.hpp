#pragma once

#include <StandardApi.hpp>
#include <filesystem>

namespace ludork::standard {

LUDORK_STANDARD_API void restoreStorage(const std::filesystem::path& directory);
LUDORK_STANDARD_API void persistStorage(const std::filesystem::path& path);

}  // namespace ludork::standard
