#pragma once

#include <SFML/Window/WindowBase.hpp>

namespace ludork::engine::platform_input {

void attachWebTouchCancellation(sf::WindowBase& window);
void detachWebTouchCancellation() noexcept;
bool consumeWebTouchCancellation();

}  // namespace ludork::engine::platform_input
