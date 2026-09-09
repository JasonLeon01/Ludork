#include "InputImpl.hpp"
#include <Input/TextInputService.hpp>
#include <Input/InputNamedValue.hpp>
#include <Input/JoystickAxisEvent.hpp>

#include <array>
#include <cmath>

namespace ludork::engine::input_impl {

namespace {

constexpr std::array DirectionalAxes = {
    sf::Joystick::Axis::X,
    sf::Joystick::Axis::PovX,
    sf::Joystick::Axis::Y,
    sf::Joystick::Axis::PovY,
};

}

std::string InputImpl::axisId(unsigned int joystickId,
                              sf::Joystick::Axis axis) {
    return std::to_string(joystickId) + ":" +
           std::to_string(static_cast<int>(axis));
}

void InputImpl::updateJoystickDominantAxes() {
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

bool InputImpl::isAnyJoystickButtonDown(unsigned int button) const {
    for (unsigned int joystickId = 0; joystickId < sf::Joystick::Count;
         ++joystickId) {
        if (sf::Joystick::isConnected(joystickId) &&
            sf::Joystick::isButtonPressed(joystickId, button)) {
            return true;
        }
    }
    return false;
}

bool InputImpl::isJoystickButtonPressed() const {
    return joystick_.buttonPressed_ && !isJoystickBlocked();
}

bool InputImpl::isJoystickButtonReleased() const {
    return joystick_.buttonReleased_ && !isJoystickBlocked();
}

bool InputImpl::getJoystickButtonPressed(unsigned int joystickId,
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

bool InputImpl::getJoystickButtonValuePressed(unsigned int joystickId,
                                              const InputNamedValue& button,
                                              bool handled) {
    return getJoystickButtonPressed(
        joystickId, static_cast<unsigned int>(button.value), handled);
}

bool InputImpl::getJoystickButtonReleased(unsigned int joystickId,
                                          unsigned int button, bool handled) {
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

bool InputImpl::getJoystickButtonValueReleased(unsigned int joystickId,
                                               const InputNamedValue& button,
                                               bool handled) {
    return getJoystickButtonReleased(
        joystickId, static_cast<unsigned int>(button.value), handled);
}

bool InputImpl::isJoystickAxisMoved() const {
    return joystick_.axisMoved_ && !isJoystickBlocked();
}

std::optional<JoystickAxisEvent> InputImpl::getJoystickAxisMoved(
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

bool InputImpl::isJoystickConnected() const {
    return joystick_.connected_ && !isJoystickBlocked();
}

bool InputImpl::isJoystickDisconnected() const {
    return joystick_.disconnected_ && !isJoystickBlocked();
}

bool InputImpl::isJoystickBlocked() const {
    return joystick_.blocked_ ||
           ludork::engine::text_input::service().blocksGameplay();
}

void InputImpl::blockJoystick() {
    joystick_.blocked_ = true;
}

void InputImpl::unblockJoystick() {
    joystick_.blocked_ = false;
}

}  // namespace ludork::engine::input_impl
