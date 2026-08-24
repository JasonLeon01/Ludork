#include "ApplicationPlatform.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace ludork::application::detail {

void configureUserDataRootEnvironment(
    const std::filesystem::path& userDataRoot) {
    if (_putenv_s("LUDORK_USER_DATA_ROOT", userDataRoot.string().c_str()) !=
        0) {
        throw std::runtime_error("Unable to configure user data root");
    }
}

}  // namespace ludork::application::detail
