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

}  // namespace ludork::engine::ui_interaction
