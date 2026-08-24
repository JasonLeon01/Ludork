#include "ApplicationPlatform.hpp"

#include <string>

namespace ludork::application::detail {

void showStartupError(const std::string& message) {
    static_cast<void>(message);
}

}  // namespace ludork::application::detail
