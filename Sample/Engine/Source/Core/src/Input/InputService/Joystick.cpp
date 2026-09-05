#include "InputRuntime.hpp"

#include <array>
#include <cmath>

namespace {

constexpr std::array DirectionalAxes = {
    sf::Joystick::Axis::X,
    sf::Joystick::Axis::PovX,
    sf::Joystick::Axis::Y,
    sf::Joystick::Axis::PovY,
};

}

std::string InputRuntime::axisId(unsigned int joystickId,
                                 sf::Joystick::Axis axis) {
    return std::to_string(joystickId) + ":" +
           std::to_string(static_cast<int>(axis));
}

void InputRuntime::updateJoystickDominantAxes() {
    for (const auto& [joystickId, axes] : joystick_.axisStatus_) {
        std::optional<sf::Joystick::Axis> dominant;
        float maximum = 0.0f;
        for (const sf::Joystick::Axis axis : DirectionalAxes) {
            const auto position = axes.find(axis);
            if (position == axes.end()) {
                continue;
            }
            const float magnitude = std::abs(position->second);
            if (magnitude > maximum) {
                maximum = magnitude;
                dominant = axis;
            }
        }
        if (maximum < 10.0f) {
            dominant.reset();
        }
        const auto previousIterator = joystick_.dominantAxis_.find(joystickId);
        const std::optional<sf::Joystick::Axis> previous =
            previousIterator == joystick_.dominantAxis_.end()
                ? std::optional<sf::Joystick::Axis>{}
                : previousIterator->second;
        if (dominant != previous) {
            if (previous.has_value()) {
                joystick_.axisTriggers_.erase(axisId(joystickId, *previous));
            }
            if (dominant.has_value()) {
                joystick_.axisTriggers_[axisId(joystickId, *dominant)] = {
                    1, false};
            }
            joystick_.dominantAxis_[joystickId] = dominant;
        }
    }
}

bool InputRuntime::isAnyJoystickButtonDown(unsigned int button) const {
    for (unsigned int joystickId = 0; joystickId < sf::Joystick::Count;
         ++joystickId) {
        if (sf::Joystick::isConnected(joystickId) &&
            sf::Joystick::isButtonPressed(joystickId, button)) {
            return true;
        }
    }
    return false;
}

bool InputRuntime::isJoystickButtonPressed() const {
    return joystick_.buttonPressed_ && !joystick_.blocked_;
}

bool InputRuntime::isJoystickButtonReleased() const {
    return joystick_.buttonReleased_ && !joystick_.blocked_;
}

bool InputRuntime::getJoystickButtonPressed(unsigned int joystickId,
                                            unsigned int button, bool handled) {
    if (!isJoystickButtonPressed()) {
        return false;
    }
    const auto joystick = joystick_.pressedEvents_.find(joystickId);
    if (joystick == joystick_.pressedEvents_.end()) {
        return false;
    }
    const auto iterator = joystick->second.find(button);
    if (iterator == joystick->second.end()) {
        return false;
    }
    const bool result = iterator->second;
    if (result && handled) {
        iterator->second = false;
    }
    return result;
}

bool InputRuntime::getJoystickButtonValuePressed(unsigned int joystickId,
                                                 const InputNamedValue& button,
                                                 bool handled) {
    return getJoystickButtonPressed(
        joystickId, static_cast<unsigned int>(button.value), handled);
}

bool InputRuntime::getJoystickButtonReleased(unsigned int joystickId,
                                             unsigned int button,
                                             bool handled) {
    if (!isJoystickButtonReleased()) {
        return false;
    }
    const auto joystick = joystick_.releasedEvents_.find(joystickId);
    if (joystick == joystick_.releasedEvents_.end()) {
        return false;
    }
    const auto iterator = joystick->second.find(button);
    if (iterator == joystick->second.end()) {
        return false;
    }
    const bool result = iterator->second;
    if (result && handled) {
        iterator->second = false;
    }
    return result;
}

bool InputRuntime::getJoystickButtonValueReleased(unsigned int joystickId,
                                                  const InputNamedValue& button,
                                                  bool handled) {
    return getJoystickButtonReleased(
        joystickId, static_cast<unsigned int>(button.value), handled);
}

bool InputRuntime::isJoystickAxisMoved() const {
    return joystick_.axisMoved_ && !joystick_.blocked_;
}

std::optional<JoystickAxisEvent> InputRuntime::getJoystickAxisMoved(
    unsigned int joystickId, bool handled) {
    if (!isJoystickAxisMoved()) {
        return std::nullopt;
    }
    const auto joystick = joystick_.axisEvents_.find(joystickId);
    if (joystick == joystick_.axisEvents_.end() || joystick->second.empty()) {
        return std::nullopt;
    }
    const auto axis = joystick->second.begin();
    const JoystickAxisEvent result{axis->first, axis->second};
    if (handled) {
        joystick->second.erase(axis);
        if (joystick->second.empty()) {
            joystick_.axisEvents_.erase(joystick);
        }
    }
    return result;
}

bool InputRuntime::isJoystickConnected() const {
    return joystick_.connected_ && !joystick_.blocked_;
}

bool InputRuntime::isJoystickDisconnected() const {
    return joystick_.disconnected_ && !joystick_.blocked_;
}

bool InputRuntime::isJoystickBlocked() const {
    return joystick_.blocked_;
}

void InputRuntime::blockJoystick() {
    joystick_.blocked_ = true;
}

void InputRuntime::unblockJoystick() {
    joystick_.blocked_ = false;
}
