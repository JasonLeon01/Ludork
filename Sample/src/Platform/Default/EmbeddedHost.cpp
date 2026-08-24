#include "ApplicationPlatform.hpp"

#include <string>

namespace ludork::application::detail {

bool configureEmbeddedHostWindow(global::RuntimeLaunchOptions& options,
                                 std::string& error) {
    static_cast<void>(options);
    error = "Embedded window mode is only supported on Windows.";
    return false;
}

}  // namespace ludork::application::detail
