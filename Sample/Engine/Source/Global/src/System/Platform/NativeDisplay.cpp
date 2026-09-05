#include "NativeDisplay.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <mutex>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include <limits>

namespace ludork::global {

#if defined(_WIN32)

namespace {

constexpr DWORD standardWindowStyle = WS_VISIBLE | WS_CAPTION | WS_MINIMIZEBOX |
                                      WS_THICKFRAME | WS_MAXIMIZEBOX |
                                      WS_SYSMENU;

void ensureProcessDpiAware() noexcept {
    static std::once_flag flag;
    std::call_once(flag, [] {
        if (const HMODULE shcore = LoadLibraryW(L"Shcore.dll")) {
            using SetProcessDpiAwareness = HRESULT(WINAPI*)(int);
            const auto setProcessDpiAwareness =
                reinterpret_cast<SetProcessDpiAwareness>(
                    GetProcAddress(shcore, "SetProcessDpiAwareness"));
            if (setProcessDpiAwareness != nullptr &&
                setProcessDpiAwareness(2) != E_INVALIDARG) {
                FreeLibrary(shcore);
                return;
            }
            FreeLibrary(shcore);
        }

        if (const HMODULE user32 = LoadLibraryW(L"user32.dll")) {
            using SetProcessDpiAware = BOOL(WINAPI*)();
            const auto setProcessDpiAware =
                reinterpret_cast<SetProcessDpiAware>(
                    GetProcAddress(user32, "SetProcessDPIAware"));
            if (setProcessDpiAware != nullptr) {
                setProcessDpiAware();
            }
            FreeLibrary(user32);
        }
    });
}

std::optional<sf::Vector2u> clientSizeForWorkArea(
    const MONITORINFO& monitorInfo) noexcept {
    RECT frame{};
    if (!AdjustWindowRectEx(&frame, standardWindowStyle, FALSE, 0)) {
        return std::nullopt;
    }

    const long long frameWidth =
        static_cast<long long>(frame.right) - frame.left;
    const long long frameHeight =
        static_cast<long long>(frame.bottom) - frame.top;

    const RECT& work = monitorInfo.rcWork;
    const long long workWidth = static_cast<long long>(work.right) - work.left;
    const long long workHeight = static_cast<long long>(work.bottom) - work.top;
    const long long clientWidth = workWidth - frameWidth;
    const long long clientHeight = workHeight - frameHeight;
    if (clientWidth <= 0 || clientHeight <= 0 ||
        clientWidth > std::numeric_limits<unsigned int>::max() ||
        clientHeight > std::numeric_limits<unsigned int>::max()) {
        return std::nullopt;
    }

    return sf::Vector2u(static_cast<unsigned int>(clientWidth),
                        static_cast<unsigned int>(clientHeight));
}

}  // namespace

std::optional<sf::Vector2u> getMaximumWindowedClientSize(
    sf::WindowHandle windowHandle) noexcept {
    ensureProcessDpiAware();

    HWND window = windowHandle;
    if (window != nullptr && !IsWindow(window)) {
        window = nullptr;
    }
    const HMONITOR monitor =
        window != nullptr ? MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST)
                          : MonitorFromPoint(POINT{}, MONITOR_DEFAULTTOPRIMARY);
    if (monitor == nullptr) {
        return std::nullopt;
    }

    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(monitor, &monitorInfo)) {
        return std::nullopt;
    }

    return clientSizeForWorkArea(monitorInfo);
}

std::optional<sf::Vector2u> getWindowedClientSize(
    sf::WindowHandle windowHandle) noexcept {
    HWND window = windowHandle;
    if (window == nullptr || !IsWindow(window)) {
        return std::nullopt;
    }
    RECT rect{};
    if (!GetClientRect(window, &rect) || rect.right <= rect.left ||
        rect.bottom <= rect.top) {
        return std::nullopt;
    }
    return sf::Vector2u(static_cast<unsigned int>(rect.right - rect.left),
                        static_cast<unsigned int>(rect.bottom - rect.top));
}

std::optional<WindowedFramePlacement> getWindowedFramePlacement(
    sf::WindowHandle) noexcept {
    return std::nullopt;
}

void setWindowedFramePlacement(sf::WindowHandle,
                               const WindowedFramePlacement&) noexcept {}

#elif !defined(__APPLE__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)

std::optional<sf::Vector2u> getMaximumWindowedClientSize(
    sf::WindowHandle) noexcept {
    return std::nullopt;
}

std::optional<sf::Vector2u> getWindowedClientSize(sf::WindowHandle) noexcept {
    return std::nullopt;
}

std::optional<WindowedFramePlacement> getWindowedFramePlacement(
    sf::WindowHandle) noexcept {
    return std::nullopt;
}

void setWindowedFramePlacement(sf::WindowHandle,
                               const WindowedFramePlacement&) noexcept {}

#endif

}  // namespace ludork::global
