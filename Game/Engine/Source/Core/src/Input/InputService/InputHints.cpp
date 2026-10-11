#include "InputImpl.hpp"
#include "InputThresholds.hpp"

#include <cmath>

namespace ludork::engine::input_impl {

InputHintDevice InputImpl::defaultInputHintDevice() {
#if defined(LUDORK_MOBILE)
    return InputHintDevice::Touch;
#else
    return InputHintDevice::KeyboardMouse;
#endif
}

InputHintDevice InputImpl::getInputHintDevice() const {
    return eventPump_.hintDevice_;
}

void InputImpl::synchronizeHintConnections() {
    for (unsigned int id = 0; id < sf::Joystick::Count; ++id) {
        const bool connected = sf::Joystick::isConnected(id);
        if (connected == eventPump_.connectedJoysticks_.contains(id)) {
            continue;
        }
        eventPump_.hintConnectionsChanged_ = true;
        if (connected) {
            eventPump_.connectedJoysticks_.insert(id);
        } else {
            eventPump_.connectedJoysticks_.erase(id);
        }
    }
}

void InputImpl::observeInputHintEvent(const sf::Event& event) {
    if (const auto* connected = event.getIf<sf::Event::JoystickConnected>()) {
        eventPump_.connectedJoysticks_.insert(connected->joystickId);
        eventPump_.hintConnectionsChanged_ = true;
        return;
    }
    if (const auto* disconnected =
            event.getIf<sf::Event::JoystickDisconnected>()) {
        eventPump_.connectedJoysticks_.erase(disconnected->joystickId);
        eventPump_.hintConnectionsChanged_ = true;
        return;
    }
    if (!eventPump_.focused_) {
        return;
    }
    std::optional<InputHintDevice> device;
    std::optional<unsigned int> joystick;
    if (const auto* button = event.getIf<sf::Event::JoystickButtonPressed>()) {
        joystick = button->joystickId;
    } else if (const auto* axis = event.getIf<sf::Event::JoystickMoved>()) {
        const bool dpad = axis->axis == sf::Joystick::Axis::PovX ||
                          axis->axis == sf::Joystick::Axis::PovY;
        if (std::isfinite(axis->position) &&
            std::abs(axis->position) >=
                (dpad ? DpadActivationThreshold : StickDeadZone)) {
            joystick = axis->joystickId;
        }
    }
    if (joystick.has_value()) {
        if (!eventPump_.connectedJoysticks_.contains(*joystick)) {
            return;
        }
        device = InputHintDevice::Gamepad;
    } else if (event.is<sf::Event::TouchBegan>() ||
               event.is<sf::Event::TouchMoved>()) {
        device = InputHintDevice::Touch;
    } else if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code != sf::Keyboard::Key::Unknown ||
            key->scancode != sf::Keyboard::Scancode::Unknown) {
            device = InputHintDevice::KeyboardMouse;
        }
    } else if (event.is<sf::Event::TextEntered>()) {
        device = InputHintDevice::KeyboardMouse;
    } else if (pointer_.touchFingers_.empty() && !pointer_.touchEnded_) {
        if (event.is<sf::Event::MouseButtonPressed>()) {
            device = InputHintDevice::KeyboardMouse;
        } else if (const auto* wheel =
                       event.getIf<sf::Event::MouseWheelScrolled>()) {
            if (std::isfinite(wheel->delta) && wheel->delta != 0.0f) {
                device = InputHintDevice::KeyboardMouse;
            }
        } else if (const auto* mouse = event.getIf<sf::Event::MouseMoved>()) {
            const sf::Vector2i position =
                eventPump_.activeWindow_
                    ? pixelToWorld(*eventPump_.activeWindow_, mouse->position)
                    : mouse->position;
            if (position != pointer_.mousePosition_) {
                device = InputHintDevice::KeyboardMouse;
            }
        }
    }
    if (device.has_value()) {
        eventPump_.hintActivities_.emplace_back(*device, joystick);
    }
}

void InputImpl::resolveInputHintDevice() {
    if (eventPump_.hintConnectionsChanged_) {
        eventPump_.hintDevice_ = eventPump_.connectedJoysticks_.empty()
                                     ? defaultInputHintDevice()
                                     : InputHintDevice::Gamepad;
    }
    for (auto activity = eventPump_.hintActivities_.rbegin();
         activity != eventPump_.hintActivities_.rend(); ++activity) {
        if (!activity->second.has_value() ||
            eventPump_.connectedJoysticks_.contains(*activity->second)) {
            eventPump_.hintDevice_ = activity->first;
            break;
        }
    }
}

}  // namespace ludork::engine::input_impl
