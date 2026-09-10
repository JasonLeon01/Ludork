#include "ApplicationPlatform.hpp"

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace ludork::application::detail {

void configureUserDataRootEnvironment(
    const std::filesystem::path& userDataRoot) {
    if (setenv("LUDORK_USER_DATA_ROOT", userDataRoot.string().c_str(), 1) !=
        0) {
        throw std::system_error(errno, std::generic_category(),
                                "Unable to configure user data root");
    }
}

}  // namespace ludork::application::detail
