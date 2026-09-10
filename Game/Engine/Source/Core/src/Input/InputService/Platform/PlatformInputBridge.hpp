#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/WindowBase.hpp>

#include <vector>

namespace ludork::engine::platform_input {

struct ScrollEvent {
    sf::Vector2f delta;
    sf::Vector2i position;
    bool precise = false;
};

void initialize();
void shutdown() noexcept;
bool setMousePosition(sf::WindowBase& window, const sf::Vector2i& position);
bool isScrollCaptureAvailable();
std::vector<ScrollEvent> consumeScrollEvents(sf::WindowBase& window);

}  // namespace ludork::engine::platform_input
