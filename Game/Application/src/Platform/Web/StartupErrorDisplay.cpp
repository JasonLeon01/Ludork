#include <ApplicationPlatform.hpp>
#include <emscripten.h>

// clang-format off
EM_JS(void, showWebStartupError, (const char* message), {
    Module['ludorkError'](UTF8ToString(message));
});
// clang-format on

namespace ludork::application::detail {

void showStartupError(const std::string& message) {
    showWebStartupError(message.c_str());
}

}  // namespace ludork::application::detail
