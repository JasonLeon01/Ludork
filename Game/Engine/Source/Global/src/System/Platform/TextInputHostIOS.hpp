#pragma once

#include <Input/TextInputHost.hpp>
#include <SFML/Window/WindowHandle.hpp>

#include <memory>

namespace ludork::global {

std::shared_ptr<ludork::engine::text_input::TextInputHost>
createIosTextInputHost(sf::WindowHandle window);

}
