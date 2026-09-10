#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/WindowHandle.hpp>

#include <optional>

namespace ludork::global {

struct WindowedFramePlacement {
    sf::Vector2i topLeft;
    sf::Vector2i screenOrigin;
    sf::Vector2u screenSize;
};

std::optional<sf::Vector2u> getMaximumWindowedClientSize(
    sf::WindowHandle windowHandle) noexcept;
std::optional<sf::Vector2u> getWindowedClientSize(
    sf::WindowHandle windowHandle) noexcept;
std::optional<WindowedFramePlacement> getWindowedFramePlacement(
    sf::WindowHandle windowHandle) noexcept;
void setWindowedFramePlacement(
    sf::WindowHandle windowHandle,
    const WindowedFramePlacement& placement) noexcept;

}  // namespace ludork::global
