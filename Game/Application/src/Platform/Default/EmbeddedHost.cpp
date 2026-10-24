#include <ApplicationPlatform.hpp>

#include <string>

namespace ludork::application::detail {

bool resolveEmbeddedHostWindow(std::uintptr_t& hostWindowHandle,
                               std::string& error) {
    static_cast<void>(hostWindowHandle);
    error = "Embedded window mode is only supported on Windows.";
    return false;
}

}  // namespace ludork::application::detail
