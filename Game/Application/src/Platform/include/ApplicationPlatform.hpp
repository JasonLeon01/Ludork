#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace ludork::application::detail {

bool resolveEmbeddedHostWindow(std::uintptr_t& hostWindowHandle,
                               std::string& error);
void configureUserDataRootEnvironment(
    const std::filesystem::path& userDataRoot);
std::filesystem::path platformBundleResourceRoot();
std::filesystem::path startupErrorLogPath();
void showStartupError(const std::string& message);

}  // namespace ludork::application::detail
