#pragma once

#include <GlobalRuntimeApi.hpp>

#include <filesystem>
#include <string>

namespace ludork::application::detail {

bool configureEmbeddedHostWindow(global::RuntimeLaunchOptions& options,
                                 std::string& error);
void configureUserDataRootEnvironment(
    const std::filesystem::path& userDataRoot);
std::filesystem::path platformBundleResourceRoot();
std::filesystem::path startupErrorLogPath();
void showStartupError(const std::string& message);
void reportStartupError(const std::string& message);

}  // namespace ludork::application::detail
