#include "ApplicationPlatform.hpp"

#include <filesystem>

namespace ludork::application::detail {

std::filesystem::path startupErrorLogPath() {
    return "Ludork-startup-error.log";
}

}  // namespace ludork::application::detail
