#include "ApplicationPlatform.hpp"

#include <cstdlib>
#include <filesystem>

namespace ludork::application::detail {

std::filesystem::path startupErrorLogPath() {
    if (const char* home = std::getenv("HOME"); home != nullptr) {
        return std::filesystem::path(home) / "Library" / "Logs" / "Ludork" /
               "Ludork-startup-error.log";
    }
    return "Ludork-startup-error.log";
}

}  // namespace ludork::application::detail
