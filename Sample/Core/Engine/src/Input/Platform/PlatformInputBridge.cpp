#include "PlatformInputBridge.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include <SFML/Window/Mouse.hpp>

#if !defined(__APPLE__) || TARGET_OS_IPHONE

namespace ludork::engine::platform_input {

void initialize() {}

void shutdown() noexcept {}

bool setMousePosition(sf::WindowBase& window, const sf::Vector2i& position) {
    sf::Mouse::setPosition(position, window);
    return sf::Mouse::getPosition(window) == position;
}

bool isScrollCaptureAvailable() {
    return false;
}

std::vector<ScrollEvent> consumeScrollEvents(sf::WindowBase&) {
    return {};
}

}  // namespace ludork::engine::platform_input

#endif
