#pragma once

#include <Input/JoystickButton.hpp>
#include <SFML/System/String.hpp>

namespace ludork::engine::joystick_device {

JoystickButton::Family appleControllerFamily(const sf::String& name);

}
