#include "ModalInputImpl.hpp"

#include <Input/JoystickButton.hpp>
#include <Runtime/WebViewService.hpp>

#include <SFML/Window/Joystick.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace ludork::engine::input_impl {

namespace {

bool isConfirmKey(sf::Keyboard::Key key, sf::Keyboard::Scancode scan) {
    return key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::Space ||
           scan == sf::Keyboard::Scancode::Enter ||
           scan == sf::Keyboard::Scancode::Space;
}

sf::Keyboard::Key nativeKey(sf::Keyboard::Key key,
                            sf::Keyboard::Scancode scan) {
    return key == sf::Keyboard::Key::Unknown &&
                   scan != sf::Keyboard::Scancode::Unknown
               ? sf::Keyboard::localize(scan)
               : key;
}

}  // namespace

bool ModalInputImpl::captured() const {
    return frameCaptured_ || waitingForRelease_ || activeCapture() != nullptr ||
           ludork::runtime::webview::isInputBlocked() ||
           revision_ != ludork::runtime::webview::inputRevision();
}

std::shared_ptr<InputCapture> ModalInputImpl::capture() {
    clearConfirmations();
    cancelTouchConfirmation();
    std::shared_ptr<InputCapture> result(new InputCapture());
    captures_.push_back(result);
    waitingForRelease_ = true;
    frameCaptured_ = true;
    synchronizeHeldConfirmControls();
    return result;
}

std::shared_ptr<InputCapture> ModalInputImpl::activeCapture() const {
    for (auto iterator = captures_.rbegin(); iterator != captures_.rend();
         ++iterator) {
        const std::shared_ptr<InputCapture> capture = iterator->lock();
        if (capture != nullptr && capture->active_) {
            return capture;
        }
    }
    return nullptr;
}

void ModalInputImpl::setConfirmEnabled(InputCapture& capture, bool enabled) {
    if (!capture.active_ || capture.confirmEnabled_ == enabled) {
        return;
    }
    capture.confirmEnabled_ = enabled;
    capture.confirmed_ = false;
    if (touchCapture_.lock().get() == &capture) {
        cancelTouchConfirmation();
    }
    if (enabled) {
        synchronizeHeldConfirmControls();
    }
}

bool ModalInputImpl::consumeConfirm(InputCapture& capture, bool focused) {
    const bool confirmed = std::exchange(capture.confirmed_, false);
    return confirmed && focused && capture.active_ && capture.confirmEnabled_ &&
           activeCapture().get() == &capture &&
           !ludork::runtime::webview::isInputBlocked() &&
           revision_ == ludork::runtime::webview::inputRevision();
}

void ModalInputImpl::confirm() {
    if (ludork::runtime::webview::isInputBlocked() ||
        revision_ != ludork::runtime::webview::inputRevision()) {
        return;
    }
    const std::shared_ptr<InputCapture> capture = activeCapture();
    if (capture != nullptr && capture->confirmEnabled_) {
        capture->confirmed_ = true;
    }
}

void ModalInputImpl::clearConfirmations() {
    std::erase_if(captures_, [](const std::weak_ptr<InputCapture>& entry) {
        const std::shared_ptr<InputCapture> capture = entry.lock();
        if (capture == nullptr || !capture->active_) {
            return true;
        }
        capture->confirmed_ = false;
        return false;
    });
}

bool ModalInputImpl::beginFrame() {
    clearConfirmations();
    const std::uint64_t revision = ludork::runtime::webview::inputRevision();
    const bool changed = revision != revision_;
    revision_ = revision;
    if (changed) {
        clearNativeControls();
        synchronizeHeldConfirmControls();
    }
    if (changed || ludork::runtime::webview::isInputBlocked() ||
        activeCapture() != nullptr) {
        waitingForRelease_ = true;
    }
    frameCaptured_ = waitingForRelease_;
#if defined(__ANDROID__)
    synchronizeSuppressedControls(changed ||
                                  ludork::runtime::webview::isInputBlocked() ||
                                  activeCapture() != nullptr);
#endif
    return changed;
}

void ModalInputImpl::finishFrame() {
    if (waitingForRelease_ && activeCapture() == nullptr &&
        !ludork::runtime::webview::isInputBlocked() &&
        revision_ == ludork::runtime::webview::inputRevision() &&
        injectedKeys_.empty() && injectedButtons_.empty() &&
        nativeTouches_.empty() && !physicalInputHeld()) {
        waitingForRelease_ = false;
    }
}

void ModalInputImpl::observeInjectedEvent(const InjectedInputEvent& event,
                                          sf::Keyboard::Key key,
                                          sf::Keyboard::Scancode scan,
                                          sf::Mouse::Button button,
                                          bool acceptsConfirmation) {
    const std::string identifier =
        scan != sf::Keyboard::Scancode::Unknown
            ? "scan:" + std::to_string(static_cast<int>(scan))
            : "key:" + std::to_string(static_cast<int>(key));
    if (event.type == "KeyPressed") {
        if (key == sf::Keyboard::Key::Unknown &&
            scan == sf::Keyboard::Scancode::Unknown) {
            return;
        }
        const bool fresh = injectedKeys_.insert(identifier).second;
        if (fresh && acceptsConfirmation && !event.alt && !event.control &&
            !event.shift && !event.system && isConfirmKey(key, scan)) {
            confirm();
        }
    } else if (event.type == "KeyReleased") {
        injectedKeys_.erase(identifier);
    } else if (event.type == "MouseButtonPressed") {
        const bool fresh = injectedButtons_.insert(button).second;
        if (fresh && acceptsConfirmation && button == sf::Mouse::Button::Left) {
            confirm();
        }
    } else if (event.type == "MouseButtonReleased") {
        injectedButtons_.erase(button);
    } else if (event.type == "FocusLost") {
        injectedKeys_.clear();
        injectedButtons_.clear();
        clearNativeControls();
        clearConfirmations();
    } else if (event.type == "FocusGained") {
        clearNativeControls();
        synchronizeHeldConfirmControls();
    }
}

bool ModalInputImpl::observeNativeEvent(const sf::Event& event,
                                        bool acceptsKeyboardMouse,
                                        bool acceptsJoystick,
                                        bool acceptsPointer) {
    observeNativeConfirmation(event, acceptsKeyboardMouse, acceptsJoystick,
                              acceptsPointer);
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

void ModalInputImpl::observeNativeConfirmation(const sf::Event& event,
                                               bool acceptsKeyboardMouse,
                                               bool acceptsJoystick,
                                               bool acceptsPointer) {
    if (event.is<sf::Event::FocusLost>()) {
        clearNativeControls();
        clearConfirmations();
    } else if (event.is<sf::Event::FocusGained>()) {
        clearNativeControls();
        synchronizeHeldConfirmControls();
    }
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        const sf::Keyboard::Key code = nativeKey(key->code, key->scancode);
        const bool held =
            nativeKeys_.contains(code) || nativeScans_.contains(key->scancode);
        if (code != sf::Keyboard::Key::Unknown) {
            nativeKeys_.insert(code);
        }
        if (key->scancode != sf::Keyboard::Scancode::Unknown) {
            nativeScans_.insert(key->scancode);
        }
        if (!held && acceptsKeyboardMouse && !key->alt && !key->control &&
            !key->shift && !key->system && isConfirmKey(code, key->scancode)) {
            confirm();
        }
    } else if (const auto* key = event.getIf<sf::Event::KeyReleased>()) {
        nativeKeys_.erase(nativeKey(key->code, key->scancode));
        nativeScans_.erase(key->scancode);
    } else if (const auto* mouse =
                   event.getIf<sf::Event::MouseButtonPressed>()) {
        const bool fresh = nativeMouseButtons_.insert(mouse->button).second;
        if (fresh && acceptsKeyboardMouse && acceptsPointer &&
            mouse->button == sf::Mouse::Button::Left) {
            confirm();
        }
    } else if (const auto* mouse =
                   event.getIf<sf::Event::MouseButtonReleased>()) {
        nativeMouseButtons_.erase(mouse->button);
    } else if (const auto* button =
                   event.getIf<sf::Event::JoystickButtonPressed>()) {
        const bool fresh = nativeJoystickButtons_[button->joystickId]
                               .insert(button->button)
                               .second;
        if (fresh && acceptsJoystick) {
            const std::optional<unsigned int> confirmButton =
                JoystickButton::resolve(button->joystickId,
                                        JoystickButton::getA());
            if (confirmButton == button->button) {
                confirm();
            }
        }
    } else if (const auto* button =
                   event.getIf<sf::Event::JoystickButtonReleased>()) {
        const auto device = nativeJoystickButtons_.find(button->joystickId);
        if (device != nativeJoystickButtons_.end()) {
            device->second.erase(button->button);
        }
    } else if (const auto* device =
                   event.getIf<sf::Event::JoystickDisconnected>()) {
        nativeJoystickButtons_.erase(device->joystickId);
    }
}

void ModalInputImpl::observeNativeTouch(const sf::Event& event,
                                        const sf::Vector2i& position,
                                        bool acceptsConfirmation,
                                        float dragThreshold) {
    if (const auto* touch = event.getIf<sf::Event::TouchBegan>()) {
        const bool wasEmpty = nativeTouches_.empty();
        if (!nativeTouches_.insert(touch->finger).second) {
            return;
        }
        if (!wasEmpty) {
            cancelTouchConfirmation();
            return;
        }
        const std::shared_ptr<InputCapture> capture = activeCapture();
        if (acceptsConfirmation && capture != nullptr &&
            capture->confirmEnabled_ &&
            !ludork::runtime::webview::isInputBlocked() &&
            revision_ == ludork::runtime::webview::inputRevision()) {
            touchCapture_ = capture;
            touchFinger_ = touch->finger;
            touchPosition_ = position;
            touchTravelDistance_ = 0.0f;
        }
    } else if (const auto* touch = event.getIf<sf::Event::TouchMoved>()) {
        if (touchFinger_ == touch->finger) {
            if (acceptsConfirmation) {
                updateTouchTravel(position, dragThreshold);
            } else {
                cancelTouchConfirmation();
            }
        }
    } else if (const auto* touch = event.getIf<sf::Event::TouchEnded>()) {
        nativeTouches_.erase(touch->finger);
        if (touchFinger_ != touch->finger) {
            return;
        }
        updateTouchTravel(position, dragThreshold);
        const std::shared_ptr<InputCapture> capture = touchCapture_.lock();
        cancelTouchConfirmation();
        if (acceptsConfirmation && nativeTouches_.empty() &&
            capture != nullptr && capture == activeCapture()) {
            confirm();
        }
    }
}

void ModalInputImpl::cancelTouchConfirmation() {
    touchCapture_.reset();
    touchFinger_.reset();
    touchTravelDistance_ = 0.0f;
}

void ModalInputImpl::updateTouchTravel(const sf::Vector2i& position,
                                       float dragThreshold) {
    const sf::Vector2i delta = position - touchPosition_;
    touchPosition_ = position;
    touchTravelDistance_ +=
        std::hypot(static_cast<float>(delta.x), static_cast<float>(delta.y));
    if (touchTravelDistance_ > dragThreshold) {
        cancelTouchConfirmation();
    }
}

void ModalInputImpl::clearNativeControls() {
    nativeKeys_.clear();
    nativeScans_.clear();
    nativeMouseButtons_.clear();
    nativeJoystickButtons_.clear();
    nativeTouches_.clear();
    cancelTouchConfirmation();
}

void ModalInputImpl::synchronizeHeldConfirmControls() {
    constexpr std::array keys = {sf::Keyboard::Key::Enter,
                                 sf::Keyboard::Key::Space};
    for (const sf::Keyboard::Key key : keys) {
        if (sf::Keyboard::isKeyPressed(key)) {
            nativeKeys_.insert(key);
            const sf::Keyboard::Scancode scan = sf::Keyboard::delocalize(key);
            if (scan != sf::Keyboard::Scancode::Unknown) {
                nativeScans_.insert(scan);
            }
        }
    }
    constexpr std::array scans = {sf::Keyboard::Scancode::Enter,
                                  sf::Keyboard::Scancode::Space};
    for (const sf::Keyboard::Scancode scan : scans) {
        if (sf::Keyboard::isKeyPressed(scan)) {
            nativeScans_.insert(scan);
            const sf::Keyboard::Key key = sf::Keyboard::localize(scan);
            if (key != sf::Keyboard::Key::Unknown) {
                nativeKeys_.insert(key);
            }
        }
    }
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        nativeMouseButtons_.insert(sf::Mouse::Button::Left);
    }
    for (unsigned int id = 0; id < sf::Joystick::Count; ++id) {
        if (!sf::Joystick::isConnected(id)) {
            nativeJoystickButtons_.erase(id);
            continue;
        }
        const std::optional<unsigned int> button =
            JoystickButton::resolve(id, JoystickButton::getA());
        if (button.has_value() && sf::Joystick::isButtonPressed(id, *button)) {
            nativeJoystickButtons_[id].insert(*button);
        }
    }
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
    for (const std::weak_ptr<InputCapture>& entry : captures_) {
        if (const std::shared_ptr<InputCapture> capture = entry.lock()) {
            capture->release();
        }
    }
    captures_.clear();
    revision_ = ludork::runtime::webview::inputRevision();
    waitingForRelease_ = false;
    frameCaptured_ = false;
    suppressNextText_ = false;
    injectedKeys_.clear();
    injectedButtons_.clear();
    clearNativeControls();
    suppressedKeys_.clear();
    suppressedScans_.clear();
    suppressedMouseButtons_.clear();
    suppressedJoystickButtons_.clear();
    suppressedJoystickAxes_.clear();
}

bool ModalInputImpl::physicalInputHeld() const {
#if defined(__ANDROID__)
    return !nativeKeys_.empty() || !nativeScans_.empty() ||
           !nativeMouseButtons_.empty() ||
           std::any_of(nativeJoystickButtons_.begin(),
                       nativeJoystickButtons_.end(), [](const auto& entry) {
                           return !entry.second.empty();
                       });
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
