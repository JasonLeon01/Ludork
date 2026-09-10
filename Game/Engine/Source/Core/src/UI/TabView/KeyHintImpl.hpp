#pragma once

#include <Input/InputNamedValue.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <string>

namespace ludork::engine::tab_view_impl {

std::string keyboardKeyText(sf::Keyboard::Key key, const std::string& source);
std::string handleKeyText(const InputNamedValue& button,
                          const std::string& source);
bool keyboardHintsAvailableWithoutJoystick();

}  // namespace ludork::engine::tab_view_impl
