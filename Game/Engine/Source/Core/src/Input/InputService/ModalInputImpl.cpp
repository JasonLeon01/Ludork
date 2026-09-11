#include "ModalInputImpl.hpp"

#include <Runtime/WebViewService.hpp>

#include <SFML/Window/Joystick.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include <array>
#include <cmath>
#include <utility>

namespace ludork::engine::input_impl {

bool ModalInputImpl::captured() const {
    return frameCaptured_ || waitingForRelease_ ||
           ludork::runtime::webview::isInputBlocked() ||
           revision_ != ludork::runtime::webview::inputRevision();
}

bool ModalInputImpl::beginFrame() {
    const std::uint64_t revision = ludork::runtime::webview::inputRevision();
    const bool changed = revision != revision_;
    revision_ = revision;
    if (changed || ludork::runtime::webview::isInputBlocked()) {
        waitingForRelease_ = true;
    }
    frameCaptured_ = waitingForRelease_;
#if defined(__ANDROID__)
    synchronizeSuppressedControls(changed ||
                                  ludork::runtime::webview::isInputBlocked());
#endif
    return changed;
}

void ModalInputImpl::finishFrame() {
    if (waitingForRelease_ && !ludork::runtime::webview::isInputBlocked() &&
        revision_ == ludork::runtime::webview::inputRevision() &&
        injectedKeys_.empty() && injectedButtons_.empty() &&
        !physicalInputHeld()) {
        waitingForRelease_ = false;
    }
}

void ModalInputImpl::observeInjectedEvent(const InjectedInputEvent& event) {
    const std::string key = event.scan >= 0
                                ? "scan:" + std::to_string(event.scan)
                                : "key:" + event.key;
    if (event.type == "KeyPressed") {
        injectedKeys_.insert(key);
    } else if (event.type == "KeyReleased") {
        injectedKeys_.erase(key);
    } else if (event.type == "MouseButtonPressed") {
        injectedButtons_.insert(event.button);
    } else if (event.type == "MouseButtonReleased") {
        injectedButtons_.erase(event.button);
    } else if (event.type == "FocusLost") {
        injectedKeys_.clear();
        injectedButtons_.clear();
    }
}

bool ModalInputImpl::observeNativeEvent(const sf::Event& event) {
#if defined(__ANDROID__)
    const bool capture = captured();
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        suppressNextText_ = false;
        if (capture) {
            suppressKey(key->code);
            if (key->scancode != sf::Keyboard::Scancode::Unknown) {
                suppressedScans_.insert(key->scancode);
            }
        }
        return !capture && !suppressedKeys_.contains(key->code) &&
               !suppressedScans_.contains(key->scancode);
    }
    if (const auto* key = event.getIf<sf::Event::KeyReleased>()) {
        const bool suppressedKey = suppressedKeys_.erase(key->code) != 0;
        const bool suppressedScan = suppressedScans_.erase(key->scancode) != 0;
        suppressNextText_ = capture || suppressedKey || suppressedScan;
        return !suppressNextText_;
    }
    if (event.is<sf::Event::TextEntered>()) {
        const bool suppressed = std::exchange(suppressNextText_, false);
        return !capture && !suppressed;
    }
    if (const auto* mouse = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (capture) {
            suppressedMouseButtons_.insert(mouse->button);
        }
        return !capture && allowsMouseButton(mouse->button);
    }
    if (const auto* mouse = event.getIf<sf::Event::MouseButtonReleased>()) {
        const bool suppressed =
            suppressedMouseButtons_.erase(mouse->button) != 0;
        return !capture && !suppressed;
    }
    if (const auto* button = event.getIf<sf::Event::JoystickButtonPressed>()) {
        if (capture) {
            suppressedJoystickButtons_[button->joystickId].insert(
                button->button);
        }
        return !capture &&
               allowsJoystickButton(button->joystickId, button->button);
    }
    if (const auto* button = event.getIf<sf::Event::JoystickButtonReleased>()) {
        const auto device = suppressedJoystickButtons_.find(button->joystickId);
        const bool suppressed = device != suppressedJoystickButtons_.end() &&
                                device->second.erase(button->button) != 0;
        return !capture && !suppressed;
    }
    if (const auto* axis = event.getIf<sf::Event::JoystickMoved>()) {
        const bool neutral = std::abs(axis->position) <= 10.0f;
        auto& suppressed = suppressedJoystickAxes_[axis->joystickId];
        if (neutral) {
            const bool previous = suppressed.erase(axis->axis) != 0;
            return !capture && !previous;
        }
        if (capture) {
            suppressed.insert(axis->axis);
        }
        return !capture && !suppressed.contains(axis->axis);
    }
    if (const auto* device = event.getIf<sf::Event::JoystickDisconnected>()) {
        suppressedJoystickButtons_.erase(device->joystickId);
        suppressedJoystickAxes_.erase(device->joystickId);
    }
#else
    static_cast<void>(event);
#endif
    return true;
}

void ModalInputImpl::suppressKey(sf::Keyboard::Key key) {
#if defined(__ANDROID__)
    if (key != sf::Keyboard::Key::Unknown) {
        suppressedKeys_.insert(key);
    }
#else
    static_cast<void>(key);
#endif
}

bool ModalInputImpl::allowsJoystickButton(unsigned int joystickId,
                                          unsigned int button) const {
    const auto device = suppressedJoystickButtons_.find(joystickId);
    return device == suppressedJoystickButtons_.end() ||
           !device->second.contains(button);
}

bool ModalInputImpl::allowsMouseButton(sf::Mouse::Button button) const {
    return !suppressedMouseButtons_.contains(button);
}

void ModalInputImpl::synchronizeSuppressedControls(bool capture) {
    for (int code = 0; code < static_cast<int>(sf::Mouse::ButtonCount);
         ++code) {
        const auto button = static_cast<sf::Mouse::Button>(code);
        if (!sf::Mouse::isButtonPressed(button)) {
            suppressedMouseButtons_.erase(button);
        } else if (capture) {
            suppressedMouseButtons_.insert(button);
        }
    }
    for (unsigned int id = 0; id < sf::Joystick::Count; ++id) {
        if (!sf::Joystick::isConnected(id)) {
            suppressedJoystickButtons_.erase(id);
            suppressedJoystickAxes_.erase(id);
            continue;
        }
        for (unsigned int button = 0; button < sf::Joystick::getButtonCount(id);
             ++button) {
            if (!sf::Joystick::isButtonPressed(id, button)) {
                const auto device = suppressedJoystickButtons_.find(id);
                if (device != suppressedJoystickButtons_.end()) {
                    device->second.erase(button);
                }
            } else if (capture) {
                suppressedJoystickButtons_[id].insert(button);
            }
        }
        for (unsigned int index = 0; index < sf::Joystick::AxisCount; ++index) {
            const auto axis = static_cast<sf::Joystick::Axis>(index);
            if (!sf::Joystick::hasAxis(id, axis) ||
                std::abs(sf::Joystick::getAxisPosition(id, axis)) <= 10.0f) {
                const auto device = suppressedJoystickAxes_.find(id);
                if (device != suppressedJoystickAxes_.end()) {
                    device->second.erase(axis);
                }
            } else if (capture) {
                suppressedJoystickAxes_[id].insert(axis);
            }
        }
    }
}

void ModalInputImpl::reset() {
    revision_ = ludork::runtime::webview::inputRevision();
    waitingForRelease_ = false;
    frameCaptured_ = false;
    suppressNextText_ = false;
    injectedKeys_.clear();
    injectedButtons_.clear();
    suppressedKeys_.clear();
    suppressedScans_.clear();
    suppressedMouseButtons_.clear();
    suppressedJoystickButtons_.clear();
    suppressedJoystickAxes_.clear();
}

bool ModalInputImpl::physicalInputHeld() const {
#if defined(__ANDROID__)
    return false;
#else
    for (int code = 0; code < static_cast<int>(sf::Keyboard::ScancodeCount);
         ++code) {
        if (sf::Keyboard::isKeyPressed(
                static_cast<sf::Keyboard::Scancode>(code))) {
            return true;
        }
    }
    for (int button = 0; button < static_cast<int>(sf::Mouse::ButtonCount);
         ++button) {
        if (sf::Mouse::isButtonPressed(
                static_cast<sf::Mouse::Button>(button))) {
            return true;
        }
    }
    constexpr std::array axes = {sf::Joystick::Axis::X, sf::Joystick::Axis::Y,
                                 sf::Joystick::Axis::PovX,
                                 sf::Joystick::Axis::PovY};
    for (unsigned int id = 0; id < sf::Joystick::Count; ++id) {
        if (!sf::Joystick::isConnected(id)) {
            continue;
        }
        for (unsigned int button = 0; button < sf::Joystick::getButtonCount(id);
             ++button) {
            if (sf::Joystick::isButtonPressed(id, button)) {
                return true;
            }
        }
        for (const sf::Joystick::Axis axis : axes) {
            if (sf::Joystick::hasAxis(id, axis) &&
                std::abs(sf::Joystick::getAxisPosition(id, axis)) > 10.0f) {
                return true;
            }
        }
    }
    return false;
#endif
}

}  // namespace ludork::engine::input_impl
