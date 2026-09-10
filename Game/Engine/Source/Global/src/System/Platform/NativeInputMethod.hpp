#pragma once

#include <SFML/Window/WindowHandle.hpp>

namespace ludork::global {

bool setNativeInputMethodDisabled(sf::WindowHandle windowHandle,
                                  bool disabled) noexcept;
bool isNativeWindowLiveResizing(sf::WindowHandle windowHandle) noexcept;
void restoreNativeInputMethod() noexcept;

}  // namespace ludork::global
