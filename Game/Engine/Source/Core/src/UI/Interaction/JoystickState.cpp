#include "JoystickState.hpp"

#include <SFML/Window/Joystick.hpp>

namespace ludork::engine::ui_interaction {

bool anyJoystickConnected() {
    for (unsigned int joystickId = 0; joystickId < sf::Joystick::Count;
         ++joystickId) {
        if (sf::Joystick::isConnected(joystickId)) {
            return true;
        }
    }
    return false;
}

bool anyJoystickButtonDown(int button) {
    if (button < 0 ||
        static_cast<unsigned int>(button) >= sf::Joystick::ButtonCount) {
        return false;
    }
    const unsigned int index = static_cast<unsigned int>(button);
    for (unsigned int joystickId = 0; joystickId < sf::Joystick::Count;
         ++joystickId) {
        if (sf::Joystick::isConnected(joystickId) &&
            sf::Joystick::isButtonPressed(joystickId, index)) {
            return true;
        }
    }
    return false;
}

}  // namespace ludork::engine::ui_interaction
