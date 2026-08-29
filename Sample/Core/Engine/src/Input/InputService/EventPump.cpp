#include "InputRuntime.hpp"

#include "Platform/PlatformInputBridge.hpp"

#include <Runtime/EngineState.hpp>

#include <SFML/Window/Mouse.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string_view>
#include <utility>

namespace {

using Key = sf::Keyboard::Key;

Key resolveKeyCode(Key key, sf::Keyboard::Scancode scan) {
    if (key != Key::Unknown) {
        return key;
    }
    if (scan == sf::Keyboard::Scancode::Unknown) {
        return key;
    }
    const Key localized = sf::Keyboard::localize(scan);
    return localized == Key::Unknown ? key : localized;
}

sf::Keyboard::Scancode scanFromCode(int code) {
    if (code < 0 ||
        static_cast<unsigned int>(code) >= sf::Keyboard::ScancodeCount) {
        return sf::Keyboard::Scancode::Unknown;
    }
    return static_cast<sf::Keyboard::Scancode>(code);
}

int parseDecimal(std::string_view value) {
    if (value.empty()) {
        return -1;
    }
    int result = 0;
    for (const char character : value) {
        if (character < '0' || character > '9') {
            return -1;
        }
        result = result * 10 + character - '0';
    }
    return result;
}

constexpr float TouchDragThreshold = 8.0f;

}  // namespace

std::atomic_bool InputEventPump::pendingSystemCancel_{false};

void InputEventPump::requestSystemCancel() noexcept {
    pendingSystemCancel_.store(true, std::memory_order_release);
}

sf::Keyboard::Key InputRuntime::keyFromName(const std::string& name) {
    if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z') {
        return static_cast<Key>(static_cast<int>(Key::A) + name[0] - 'A');
    }
    if (name.size() == 4 && name.starts_with("Num") && name[3] >= '0' &&
        name[3] <= '9') {
        return static_cast<Key>(static_cast<int>(Key::Num0) + name[3] - '0');
    }
    if (name.size() == 7 && name.starts_with("Numpad") && name[6] >= '0' &&
        name[6] <= '9') {
        return static_cast<Key>(static_cast<int>(Key::Numpad0) + name[6] - '0');
    }
    if (name.starts_with('F')) {
        const int number = parseDecimal(std::string_view(name).substr(1));
        if (number >= 1 && number <= 15) {
            return static_cast<Key>(static_cast<int>(Key::F1) + number - 1);
        }
    }

    static constexpr std::array namedKeys = {
        std::pair{"Escape", Key::Escape},
        std::pair{"LControl", Key::LControl},
        std::pair{"LShift", Key::LShift},
        std::pair{"LAlt", Key::LAlt},
        std::pair{"LSystem", Key::LSystem},
        std::pair{"RControl", Key::RControl},
        std::pair{"RShift", Key::RShift},
        std::pair{"RAlt", Key::RAlt},
        std::pair{"RSystem", Key::RSystem},
        std::pair{"Menu", Key::Menu},
        std::pair{"LBracket", Key::LBracket},
        std::pair{"RBracket", Key::RBracket},
        std::pair{"Semicolon", Key::Semicolon},
        std::pair{"Comma", Key::Comma},
        std::pair{"Period", Key::Period},
        std::pair{"Apostrophe", Key::Apostrophe},
        std::pair{"Slash", Key::Slash},
        std::pair{"Backslash", Key::Backslash},
        std::pair{"Grave", Key::Grave},
        std::pair{"Equal", Key::Equal},
        std::pair{"Hyphen", Key::Hyphen},
        std::pair{"Space", Key::Space},
        std::pair{"Enter", Key::Enter},
        std::pair{"Backspace", Key::Backspace},
        std::pair{"Tab", Key::Tab},
        std::pair{"PageUp", Key::PageUp},
        std::pair{"PageDown", Key::PageDown},
        std::pair{"End", Key::End},
        std::pair{"Home", Key::Home},
        std::pair{"Insert", Key::Insert},
        std::pair{"Delete", Key::Delete},
        std::pair{"Add", Key::Add},
        std::pair{"Subtract", Key::Subtract},
        std::pair{"Multiply", Key::Multiply},
        std::pair{"Divide", Key::Divide},
        std::pair{"Left", Key::Left},
        std::pair{"Right", Key::Right},
        std::pair{"Up", Key::Up},
        std::pair{"Down", Key::Down},
        std::pair{"Pause", Key::Pause},
    };
    const auto iterator = std::find_if(namedKeys.begin(), namedKeys.end(),
                                       [&name](const auto& entry) {
                                           return entry.first == name;
                                       });
    return iterator == namedKeys.end() ? Key::Unknown : iterator->second;
}

sf::Mouse::Button InputRuntime::mouseButtonFromName(const std::string& name) {
    if (name == "Right") {
        return sf::Mouse::Button::Right;
    }
    if (name == "Middle") {
        return sf::Mouse::Button::Middle;
    }
    if (name == "Extra1") {
        return sf::Mouse::Button::Extra1;
    }
    if (name == "Extra2") {
        return sf::Mouse::Button::Extra2;
    }
    return sf::Mouse::Button::Left;
}

std::string InputRuntime::toUtf8(char32_t codepoint) {
    if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        codepoint = 0xFFFD;
    }
    std::string result;
    if (codepoint <= 0x7F) {
        result.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    return result;
}

void InputRuntime::consumePendingSystemCancel() {
    if (!InputEventPump::pendingSystemCancel_.exchange(
            false, std::memory_order_acq_rel)) {
        return;
    }
    abortTwoFingerCancel();
    cancelTouchGesture();
    setKeyPulse(Key::Escape, sf::Keyboard::Scancode::Escape, {});
}

void InputRuntime::setFocused(bool focused) {
    if (eventPump_.focused_ == focused) {
        return;
    }
    eventPump_.focused_ = focused;
    if (focused) {
        eventPump_.focusGained_ = true;
        return;
    }
    eventPump_.focusLost_ = true;
    abortTwoFingerCancel();
    pointer_.touchTravelDistance_ = 0.0f;
    pointer_.touchDragged_ = false;
    pointer_.touchGestureSuppressed_ = true;
    pointer_.primaryTouchFinger_.reset();
    pointer_.touchFingers_.clear();
    pointer_.touchActive_ = false;
    pointer_.touchTrigger_ = {};
    clearKeyboardState();
}

void InputRuntime::injectEvent(const InjectedInputEvent& event) {
    const std::lock_guard<std::mutex> lock(eventPump_.injectedEventsMutex_);
    eventPump_.injectedEvents_.push_back(event);
}

void InputRuntime::setUseInjectedMouseOnly(bool value) {
    eventPump_.useInjectedMouseOnly_ = value;
    if (!value) {
        pointer_.injectedPixel_.reset();
        pointer_.injectedTransitionPending_.reset();
    }
}

void InputRuntime::setPointerViewport(std::optional<sf::IntRect> viewport) {
    pointer_.viewport_ = std::move(viewport);
    if (!eventPump_.useInjectedMouseOnly_ ||
        !pointer_.injectedPixel_.has_value() ||
        eventPump_.activeWindow_ == nullptr) {
        return;
    }
    const bool inside = acceptsPointerPixel(*pointer_.injectedPixel_);
    if (!pointer_.injectedTransitionPending_.has_value()) {
        if (inside != pointer_.insideViewport_) {
            pointer_.injectedTransitionPending_ = inside;
        }
    } else if (inside == !*pointer_.injectedTransitionPending_) {
        pointer_.injectedTransitionPending_.reset();
    }
    pointer_.insideViewport_ = inside;
    pointer_.mousePosition_ =
        pixelToWorld(*eventPump_.activeWindow_, *pointer_.injectedPixel_);
}

void InputRuntime::onWindowRecreated(sf::WindowBase& window) {
    resetFrameState();
    clearKeyboardState();
    pointer_.mouseButtonPressed_ = false;
    pointer_.mouseButtonReleased_ = false;
    pointer_.mousePressedEvents_.clear();
    pointer_.mouseReleasedEvents_.clear();
    pointer_.mouseTriggers_.clear();
    pointer_.pendingMouseTriggerReleases_.clear();
    pointer_.mouseMoved_ = false;
    pointer_.mouseMovedDelta_.reset();
    pointer_.mouseEntered_ = false;
    pointer_.mouseLeft_ = false;
    pointer_.touchBegan_ = false;
    pointer_.touchEnded_ = false;
    pointer_.touchMoved_ = false;
    pointer_.touchActive_ = false;
    pointer_.touchPosition_.reset();
    pointer_.touchBeganPosition_.reset();
    pointer_.touchTap_ = false;
    pointer_.touchTapHandled_ = false;
    pointer_.touchTapPosition_.reset();
    pointer_.touchEndedPosition_.reset();
    pointer_.touchMovedDelta_.reset();
    pointer_.touchBeganHandled_ = false;
    pointer_.touchTrigger_ = {};
    pointer_.touchTravelDistance_ = 0.0f;
    pointer_.touchDragged_ = false;
    pointer_.touchGestureSuppressed_ = false;
    pointer_.primaryTouchFinger_.reset();
    pointer_.touchFingers_.clear();
    pointer_.touchCancelMouseActive_ = false;
    pointer_.touchCancelMousePressedThisFrame_ = false;
    pointer_.touchCancelMouseReleasePending_.reset();
    pointer_.injectedPixel_.reset();
    pointer_.injectedTransitionPending_.reset();
    eventPump_.focused_ = window.hasFocus();
    eventPump_.activeWindow_ = &window;
    pointer_.insideViewport_ =
        acceptsPointerPixel(sf::Mouse::getPosition(window));
    window.setMouseCursorVisible(eventPump_.currentInputType_ ==
                                 InputType::Mouse);
}

void InputRuntime::processPlatformScrollEvents(sf::WindowBase& window) {
    if (!ludork::engine::platform_input::isScrollCaptureAvailable()) {
        return;
    }
    const std::vector<ludork::engine::platform_input::ScrollEvent> events =
        ludork::engine::platform_input::consumeScrollEvents(window);
    if (events.empty()) {
        return;
    }
    pointer_.mouseWheelScrolled_ = false;
    pointer_.mouseWheel_.reset();
    pointer_.mouseWheelDelta_ = 0.0f;
    pointer_.mouseWheelPrecise_ = false;
    pointer_.mouseWheelPosition_.reset();
    if (eventPump_.useInjectedMouseOnly_ || !window.hasFocus()) {
        return;
    }
    for (const ludork::engine::platform_input::ScrollEvent& event : events) {
        updatePointerViewportState(event.position);
        if (!acceptsPointerPixel(event.position)) {
            continue;
        }
        const sf::Vector2i position = pixelToWorld(window, event.position);
        recordMouseWheel(sf::Mouse::Wheel::Vertical, event.delta.y, position,
                         event.precise);
        recordMouseWheel(sf::Mouse::Wheel::Horizontal, event.delta.x, position,
                         event.precise);
    }
}

void InputRuntime::processInjectedEvents() {
    std::deque<InjectedInputEvent> events;
    {
        const std::lock_guard<std::mutex> lock(eventPump_.injectedEventsMutex_);
        events.swap(eventPump_.injectedEvents_);
    }
    while (!events.empty()) {
        const InjectedInputEvent event = std::move(events.front());
        events.pop_front();
        const InputModifiers modifiers{event.alt, event.control, event.shift,
                                       event.system};
        const sf::Vector2i pixel{event.x, event.y};
        const sf::Vector2i position =
            eventPump_.activeWindow_ == nullptr
                ? pixel
                : pixelToWorld(*eventPump_.activeWindow_, pixel);
        if (event.type == "MouseMoved" || event.type == "MouseButtonPressed" ||
            event.type == "MouseButtonReleased" ||
            event.type == "MouseWheelScrolled" ||
            event.type == "MouseEntered") {
            pointer_.injectedPixel_ = pixel;
        }
        if (event.type == "KeyPressed") {
            setFocused(true);
            sf::Keyboard::Key key = keyFromName(event.key);
            sf::Keyboard::Scancode scan = scanFromCode(event.scan);
            if (scan == sf::Keyboard::Scancode::Unknown &&
                key != sf::Keyboard::Key::Unknown) {
                scan = sf::Keyboard::delocalize(key);
            }
            key = resolveKeyCode(key, scan);
            setKeyPressed(key, scan, modifiers);
        } else if (event.type == "KeyReleased") {
            setFocused(true);
            sf::Keyboard::Key key = keyFromName(event.key);
            sf::Keyboard::Scancode scan = scanFromCode(event.scan);
            if (scan == sf::Keyboard::Scancode::Unknown &&
                key != sf::Keyboard::Key::Unknown) {
                scan = sf::Keyboard::delocalize(key);
            }
            key = resolveKeyCode(key, scan);
            setKeyReleased(key, scan, modifiers);
        } else if (event.type == "MouseMoved") {
            updatePointerViewportState(pixel);
            if (acceptsPointerPixel(pixel) ||
                !pointer_.mouseTriggers_.empty()) {
                const sf::Vector2i previous = pointer_.mousePosition_;
                pointer_.mouseMoved_ = true;
                pointer_.mousePosition_ = position;
                if (pointer_.mousePosition_ != previous) {
                    pointer_.mouseMovedDelta_ =
                        pointer_.mousePosition_ - previous;
                }
            } else {
                pointer_.mousePosition_ = position;
            }
        } else if (event.type == "MouseButtonPressed") {
            updatePointerViewportState(pixel);
            if (acceptsPointerPixel(pixel)) {
                setMouseButtonPressed(mouseButtonFromName(event.button),
                                      position);
            }
        } else if (event.type == "MouseButtonReleased") {
            updatePointerViewportState(pixel);
            const sf::Mouse::Button button = mouseButtonFromName(event.button);
            if (pointer_.mouseTriggers_.contains(static_cast<int>(button))) {
                setMouseButtonReleased(button, position);
            }
        } else if (event.type == "MouseWheelScrolled") {
            updatePointerViewportState(pixel);
            if (acceptsPointerPixel(pixel)) {
                recordMouseWheel(sf::Mouse::Wheel::Vertical, event.delta,
                                 position);
            }
        } else if (event.type == "FocusGained") {
            setFocused(true);
        } else if (event.type == "FocusLost") {
            setFocused(false);
        } else if (event.type == "MouseEntered") {
            updatePointerViewportState(pixel);
        } else if (event.type == "MouseLeft") {
            pointer_.injectedPixel_.reset();
            pointer_.injectedTransitionPending_.reset();
            updatePointerViewportState(false);
            if (pointer_.mouseTriggers_.empty()) {
                const int outside = std::numeric_limits<int>::lowest() / 2;
                pointer_.mousePosition_ = {outside, outside};
                pointer_.mouseMoved_ = false;
                pointer_.mouseMovedDelta_.reset();
            }
        }
    }
}

bool InputRuntime::processNativeEvent(sf::WindowBase& window,
                                      const sf::Event& event) {
    if (event.is<sf::Event::Closed>()) {
        window.close();
    }
    if (!eventPump_.useInjectedMouseOnly_ && event.is<sf::Event::FocusLost>()) {
        setFocused(false);
    }
    if (!eventPump_.useInjectedMouseOnly_ &&
        event.is<sf::Event::FocusGained>()) {
        setFocused(true);
    }
    if (!eventPump_.useInjectedMouseOnly_ && !window.hasFocus()) {
        return false;
    }

    if (!eventPump_.useInjectedMouseOnly_) {
        if (const sf::Event::KeyPressed* keyEvent =
                event.getIf<sf::Event::KeyPressed>()) {
            setKeyPressed(resolveKeyCode(keyEvent->code, keyEvent->scancode),
                          keyEvent->scancode,
                          {keyEvent->alt, keyEvent->control, keyEvent->shift,
                           keyEvent->system});
        }
        if (const sf::Event::KeyReleased* keyEvent =
                event.getIf<sf::Event::KeyReleased>()) {
            setKeyReleased(resolveKeyCode(keyEvent->code, keyEvent->scancode),
                           keyEvent->scancode,
                           {keyEvent->alt, keyEvent->control, keyEvent->shift,
                            keyEvent->system});
        }
        if (const sf::Event::MouseWheelScrolled* mouseEvent =
                event.getIf<sf::Event::MouseWheelScrolled>()) {
            updatePointerViewportState(mouseEvent->position);
            if (acceptsPointerPixel(mouseEvent->position)) {
                recordMouseWheel(mouseEvent->wheel, mouseEvent->delta,
                                 pixelToWorld(window, mouseEvent->position));
            }
        }
        if (const sf::Event::MouseButtonPressed* mouseEvent =
                event.getIf<sf::Event::MouseButtonPressed>()) {
            updatePointerViewportState(mouseEvent->position);
            if (acceptsPointerPixel(mouseEvent->position)) {
                setMouseButtonPressed(
                    mouseEvent->button,
                    pixelToWorld(window, mouseEvent->position));
            }
        }
        if (const sf::Event::MouseButtonReleased* mouseEvent =
                event.getIf<sf::Event::MouseButtonReleased>()) {
            updatePointerViewportState(mouseEvent->position);
            if (pointer_.mouseTriggers_.contains(
                    static_cast<int>(mouseEvent->button))) {
                setMouseButtonReleased(
                    mouseEvent->button,
                    pixelToWorld(window, mouseEvent->position));
            }
        }
        if (const sf::Event::MouseMoved* mouseEvent =
                event.getIf<sf::Event::MouseMoved>()) {
            updatePointerViewportState(mouseEvent->position);
            const sf::Vector2i position =
                pixelToWorld(window, mouseEvent->position);
            if (acceptsPointerPixel(mouseEvent->position) ||
                !pointer_.mouseTriggers_.empty()) {
                const sf::Vector2i previous = pointer_.mousePosition_;
                pointer_.mouseMoved_ = true;
                pointer_.mousePosition_ = position;
                if (pointer_.mousePosition_ != previous) {
                    pointer_.mouseMovedDelta_ =
                        pointer_.mousePosition_ - previous;
                }
            } else {
                pointer_.mousePosition_ = position;
            }
        }
        if (event.is<sf::Event::MouseEntered>()) {
            updatePointerViewportState(sf::Mouse::getPosition(window));
        }
        if (event.is<sf::Event::MouseLeft>()) {
            updatePointerViewportState(false);
        }
    }

    if (!pointer_.touchBlocked_) {
        if (const sf::Event::TouchBegan* touchEvent =
                event.getIf<sf::Event::TouchBegan>()) {
            if (!acceptsPointerPixel(touchEvent->position)) {
                return true;
            }
            const sf::Vector2i position =
                pixelToWorld(window, touchEvent->position);
            const bool hadTrackedTouches = !pointer_.touchFingers_.empty();
            pointer_.touchFingers_[touchEvent->finger] = position;
            if (!pointer_.touchBegan_ && !pointer_.touchEnded_ &&
                !hadTrackedTouches &&
                !pointer_.primaryTouchFinger_.has_value()) {
                pointer_.primaryTouchFinger_ = touchEvent->finger;
                pointer_.touchBegan_ = true;
                pointer_.touchActive_ = true;
                pointer_.touchBeganPosition_ = position;
                pointer_.touchPosition_ = position;
                pointer_.touchTravelDistance_ = 0.0f;
                pointer_.touchDragged_ = false;
                pointer_.touchGestureSuppressed_ = false;
                ++pointer_.touchTrigger_.count;
                pointer_.touchTrigger_.handled = false;
            }
            if (pointer_.touchFingers_.size() >= 2) {
                beginTwoFingerCancel(position);
            }
        }
        if (const sf::Event::TouchMoved* touchEvent =
                event.getIf<sf::Event::TouchMoved>()) {
            const sf::Vector2i position =
                pixelToWorld(window, touchEvent->position);
            const auto finger = pointer_.touchFingers_.find(touchEvent->finger);
            if (finger != pointer_.touchFingers_.end()) {
                finger->second = position;
            }
            if (pointer_.primaryTouchFinger_.has_value() &&
                touchEvent->finger == *pointer_.primaryTouchFinger_ &&
                finger != pointer_.touchFingers_.end() &&
                pointer_.touchActive_) {
                const std::optional<sf::Vector2i> previous =
                    pointer_.touchPosition_;
                pointer_.touchMoved_ = true;
                pointer_.touchPosition_ = position;
                if (previous.has_value() && position != *previous) {
                    const sf::Vector2i delta = position - *previous;
                    pointer_.touchMovedDelta_ = delta;
                    pointer_.touchTravelDistance_ +=
                        std::hypot(static_cast<float>(delta.x),
                                   static_cast<float>(delta.y));
                    if (pointer_.touchTravelDistance_ >
                        TouchDragThreshold * Scale) {
                        pointer_.touchDragged_ = true;
                        pointer_.touchTrigger_.handled = true;
                    }
                }
            }
        }
        if (const sf::Event::TouchEnded* touchEvent =
                event.getIf<sf::Event::TouchEnded>()) {
            const sf::Vector2i position =
                pixelToWorld(window, touchEvent->position);
            const auto finger = pointer_.touchFingers_.find(touchEvent->finger);
            const bool primaryFinger =
                pointer_.primaryTouchFinger_.has_value() &&
                touchEvent->finger == *pointer_.primaryTouchFinger_;
            const bool primaryGesture =
                primaryFinger && finger != pointer_.touchFingers_.end() &&
                pointer_.touchActive_;
            if (finger != pointer_.touchFingers_.end()) {
                pointer_.touchFingers_.erase(finger);
            }
            if (primaryGesture) {
                const std::optional<sf::Vector2i> previous =
                    pointer_.touchPosition_;
                if (previous.has_value() && position != *previous) {
                    const sf::Vector2i delta = position - *previous;
                    pointer_.touchMoved_ = true;
                    pointer_.touchMovedDelta_ = delta;
                    pointer_.touchTravelDistance_ +=
                        std::hypot(static_cast<float>(delta.x),
                                   static_cast<float>(delta.y));
                    if (pointer_.touchTravelDistance_ >
                        TouchDragThreshold * Scale) {
                        pointer_.touchDragged_ = true;
                        pointer_.touchTrigger_.handled = true;
                    }
                }
                pointer_.touchEnded_ = true;
                pointer_.touchActive_ = false;
                pointer_.touchEndedPosition_ = position;
                pointer_.touchPosition_ = position;
                if (!pointer_.touchDragged_ &&
                    !pointer_.touchGestureSuppressed_) {
                    pointer_.touchTap_ = true;
                    pointer_.touchTapPosition_ = position;
                }
                pointer_.touchTrigger_ = {};
            }
            if (primaryFinger) {
                pointer_.touchActive_ = false;
                pointer_.touchTrigger_ = {};
                pointer_.primaryTouchFinger_.reset();
            }
            if (pointer_.touchCancelMouseActive_ &&
                pointer_.touchFingers_.size() < 2) {
                endTwoFingerCancel(position);
            }
        }
    }

    if (const sf::Event::JoystickButtonPressed* joystickEvent =
            event.getIf<sf::Event::JoystickButtonPressed>()) {
        joystick_.buttonPressed_ = true;
        joystick_
            .pressedEvents_[joystickEvent->joystickId][joystickEvent->button] =
            true;
        InputTriggerEntry& entry =
            joystick_.buttonTriggers_[joystickEvent->button];
        ++entry.count;
    }
    if (const sf::Event::JoystickButtonReleased* joystickEvent =
            event.getIf<sf::Event::JoystickButtonReleased>()) {
        joystick_.buttonReleased_ = true;
        joystick_
            .releasedEvents_[joystickEvent->joystickId][joystickEvent->button] =
            true;
        joystick_.buttonTriggers_.erase(joystickEvent->button);
    }
    if (const sf::Event::JoystickMoved* joystickEvent =
            event.getIf<sf::Event::JoystickMoved>()) {
        joystick_.axisMoved_ = true;
        joystick_.axisEvents_[joystickEvent->joystickId][joystickEvent->axis] =
            joystickEvent->position;
        joystick_.axisStatus_[joystickEvent->joystickId][joystickEvent->axis] =
            joystickEvent->position;
    }
    joystick_.connected_ =
        joystick_.connected_ || event.is<sf::Event::JoystickConnected>();
    if (const sf::Event::JoystickDisconnected* joystickEvent =
            event.getIf<sf::Event::JoystickDisconnected>()) {
        joystick_.disconnected_ = true;
        joystick_.axisStatus_.erase(joystickEvent->joystickId);
        joystick_.pressedEvents_.erase(joystickEvent->joystickId);
        joystick_.releasedEvents_.erase(joystickEvent->joystickId);
    }
    if (!eventPump_.useInjectedMouseOnly_) {
        if (const sf::Event::TextEntered* textEvent =
                event.getIf<sf::Event::TextEntered>()) {
            keyboard_.enteredText_ += toUtf8(textEvent->unicode);
        }
    }
    return true;
}

void InputRuntime::initializeNativePolling() {
    if (eventPump_.useInjectedMouseOnly_) {
        return;
    }
    static_cast<void>(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A));
    ludork::engine::platform_input::initialize();
}

bool InputRuntime::isFocused() const {
    return eventPump_.focused_;
}

bool InputRuntime::isFocusLost() const {
    return eventPump_.focusLost_;
}

bool InputRuntime::isFocusGained() const {
    return eventPump_.focusGained_;
}
