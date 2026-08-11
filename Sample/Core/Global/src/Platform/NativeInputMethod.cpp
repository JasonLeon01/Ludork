#include "NativeInputMethod.hpp"

#if defined(_WIN32)
#include <windows.h>

#include <imm.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace ludork::global {

#if defined(_WIN32)

namespace {

HWND controlledWindow = nullptr;
HIMC previousInputContext = nullptr;
bool inputMethodDisabled = false;

void restoreInputContext() noexcept {
    if (!inputMethodDisabled) {
        return;
    }
    if (controlledWindow != nullptr && IsWindow(controlledWindow)) {
        ImmAssociateContext(controlledWindow, previousInputContext);
    }
    controlledWindow = nullptr;
    previousInputContext = nullptr;
    inputMethodDisabled = false;
}

}  // namespace

bool setNativeInputMethodDisabled(sf::WindowHandle windowHandle,
                                  bool disabled) noexcept {
    if (!disabled) {
        if (!inputMethodDisabled) {
            return true;
        }
        restoreInputContext();
        return true;
    }
    if (windowHandle == nullptr) {
        return false;
    }
    if (inputMethodDisabled && controlledWindow == windowHandle) {
        return true;
    }
    restoreInputContext();
    controlledWindow = windowHandle;
    previousInputContext = ImmAssociateContext(controlledWindow, nullptr);
    inputMethodDisabled = true;
    return true;
}

bool isNativeWindowLiveResizing(sf::WindowHandle) noexcept {
    return false;
}

void restoreNativeInputMethod() noexcept {
    restoreInputContext();
}

#elif !defined(__APPLE__) || TARGET_OS_IPHONE

bool setNativeInputMethodDisabled(sf::WindowHandle, bool) noexcept {
    return false;
}

bool isNativeWindowLiveResizing(sf::WindowHandle) noexcept {
    return false;
}

void restoreNativeInputMethod() noexcept {}

#endif

}  // namespace ludork::global
