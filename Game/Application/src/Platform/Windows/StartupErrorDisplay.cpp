#include "ApplicationPlatform.hpp"

#include <cstdlib>
#include <string>

#include <windows.h>

namespace {

std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring(value.begin(), value.end());
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(),
                        static_cast<int>(value.size()), result.data(), size);
    return result;
}

}  // namespace

namespace ludork::application::detail {

void showStartupError(const std::string& message) {
    if (std::getenv("LUDORK_EDITOR") == nullptr) {
        const std::wstring text = utf8ToWide(message);
        MessageBoxW(nullptr, text.c_str(), L"Ludork startup error",
                    MB_OK | MB_ICONERROR);
    }
}

}  // namespace ludork::application::detail
