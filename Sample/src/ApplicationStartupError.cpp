#include "ApplicationPlatform.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace ludork::application::detail {

void reportStartupError(const std::string& message) {
    std::fprintf(stderr, "%s\n", message.c_str());
    const std::filesystem::path logPath = startupErrorLogPath();
    if (!logPath.parent_path().empty()) {
        std::error_code error;
        std::filesystem::create_directories(logPath.parent_path(), error);
    }
    std::ofstream log(logPath, std::ios::app);
    if (log) {
        log << message << '\n';
    }
    showStartupError(message);
}

}  // namespace ludork::application::detail
