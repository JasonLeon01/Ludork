#pragma once

#include <filesystem>

namespace ludork::application::detail {

std::filesystem::path normalizedAbsolutePath(const std::filesystem::path& path);
std::filesystem::path resolveLuaScriptPath(const std::filesystem::path& path);
bool useRuntimeRoot(const std::filesystem::path& executablePath,
                    std::filesystem::path& runtimeRoot);

}  // namespace ludork::application::detail
