#pragma once

#include <SFML/Window/WindowHandle.hpp>

namespace ludork::global {

bool setNativeInputMethodDisabled(sf::WindowHandle windowHandle,
                                  bool disabled) noexcept;
void restoreNativeInputMethod() noexcept;

}  // namespace ludork::global
