#pragma once

#include <Input/TextInputHost.hpp>
#include <SFML/Window/WindowBase.hpp>
#include <memory>

namespace ludork::global {

std::shared_ptr<ludork::engine::text_input::TextInputHost>
createDesktopTextInputHost(sf::WindowBase& window);

}  // namespace ludork::global
