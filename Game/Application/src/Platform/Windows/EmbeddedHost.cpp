#include "ApplicationPlatform.hpp"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include <windows.h>

namespace ludork::application::detail {

bool configureEmbeddedHostWindow(global::RuntimeLaunchOptions& options,
                                 std::string& error) {
    const char* handle = std::getenv("LUDORK_WINDOW_HANDLE");
    const std::string_view handleValue = handle == nullptr ? "" : handle;
    if (handleValue.empty()) {
        error = "LUDORK_WINDOW_HANDLE is required for embedded mode.";
        return false;
    }
    std::uintptr_t parsedHandle = 0;
    const std::from_chars_result result = std::from_chars(
        handleValue.data(), handleValue.data() + handleValue.size(),
        parsedHandle, 10);
    if (result.ec != std::errc{} ||
        result.ptr != handleValue.data() + handleValue.size() ||
        parsedHandle == 0) {
        error =
            "LUDORK_WINDOW_HANDLE must be a non-zero decimal window handle.";
        return false;
    }
    if (!IsWindow(reinterpret_cast<HWND>(parsedHandle))) {
        error = "LUDORK_WINDOW_HANDLE does not identify a valid window.";
        return false;
    }
    options.hostWindowHandle = parsedHandle;
    return true;
}

}  // namespace ludork::application::detail
