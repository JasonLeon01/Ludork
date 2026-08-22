#include "InputRuntime.hpp"

#include "Platform/PlatformInputBridge.hpp"

#include <SFML/Window/Clipboard.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

void InputRuntime::resetFrameState() {
    restoreKeyPulses();
    for (const std::string& identifier : keyboard_.pendingKeyTriggerReleases_) {
        keyboard_.keyTriggers_.erase(identifier);
    }
    for (const std::string& identifier :
         keyboard_.pendingScanTriggerReleases_) {
        keyboard_.scanTriggers_.erase(identifier);
    }
    for (const int button : pointer_.pendingMouseTriggerReleases_) {
        pointer_.mouseTriggers_.erase(button);
    }
    keyboard_.pendingKeyTriggerReleases_.clear();
    keyboard_.pendingScanTriggerReleases_.clear();
    pointer_.pendingMouseTriggerReleases_.clear();
    eventPump_.focusLost_ = false;
    eventPump_.focusGained_ = false;
    keyboard_.keyPressed_ = false;
    keyboard_.keyReleased_ = false;
    keyboard_.keyPressedEvents_.clear();
    keyboard_.keyReleasedEvents_.clear();
    keyboard_.scanPressedEvents_.clear();
    keyboard_.scanReleasedEvents_.clear();
    pointer_.mouseWheelScrolled_ = false;
    pointer_.mouseWheel_.reset();
    pointer_.mouseWheelDelta_ = 0.0f;
    pointer_.mouseWheelPrecise_ = false;
    pointer_.mouseWheelPosition_.reset();
    pointer_.mouseButtonPressed_ = false;
    pointer_.mouseButtonReleased_ = false;
    pointer_.mousePressedEvents_.clear();
    pointer_.mouseReleasedEvents_.clear();
    pointer_.mouseMoved_ = false;
    pointer_.mouseMovedDelta_.reset();
    pointer_.mouseEntered_ = false;
    pointer_.mouseLeft_ = false;
    pointer_.touchBegan_ = false;
    pointer_.touchEnded_ = false;
    pointer_.touchMoved_ = false;
    pointer_.touchBeganPosition_.reset();
    pointer_.touchTap_ = false;
    pointer_.touchTapHandled_ = false;
    pointer_.touchTapPosition_.reset();
    pointer_.touchEndedPosition_.reset();
    pointer_.touchMovedDelta_.reset();
    pointer_.touchBeganHandled_ = false;
    pointer_.touchCancelMousePressedThisFrame_ = false;
    joystick_.buttonPressed_ = false;
    joystick_.buttonReleased_ = false;
    joystick_.axisMoved_ = false;
    joystick_.connected_ = false;
    joystick_.disconnected_ = false;
    joystick_.pressedEvents_.clear();
    joystick_.releasedEvents_.clear();
    joystick_.axisEvents_.clear();
    keyboard_.enteredText_.clear();
}

void InputRuntime::updateInputType(sf::WindowBase& window) {
    std::optional<InputType> next;
    if (pointer_.mouseMoved_ || pointer_.mouseButtonPressed_ ||
        pointer_.mouseWheelScrolled_) {
        next = InputType::Mouse;
    } else if (keyboard_.keyPressed_ || joystick_.buttonPressed_ ||
               !joystick_.axisTriggers_.empty()) {
        next = InputType::Gamepad;
    } else if (joystick_.axisMoved_) {
        for (const auto& [joystickId, axes] : joystick_.axisStatus_) {
            static_cast<void>(joystickId);
            if (std::any_of(axes.begin(), axes.end(), [](const auto& entry) {
                    return std::abs(entry.second) > 10.0f;
                })) {
                next = InputType::Gamepad;
                break;
            }
        }
    }
    if (next.has_value() && *next != eventPump_.currentInputType_) {
        eventPump_.currentInputType_ = *next;
        window.setMouseCursorVisible(eventPump_.currentInputType_ ==
                                     InputType::Mouse);
    }
}

void InputRuntime::update(sf::WindowBase& window) {
    eventPump_.activeWindow_ = &window;
    resetFrameState();
    consumePendingSystemCancel();
    if (pointer_.injectedTransitionPending_.has_value()) {
        if (*pointer_.injectedTransitionPending_) {
            pointer_.mouseEntered_ = true;
        } else {
            pointer_.mouseLeft_ = true;
        }
        pointer_.injectedTransitionPending_.reset();
    }
    releasePendingTwoFingerCancel();
    processInjectedEvents();
    if (!eventPump_.useInjectedMouseOnly_) {
        setFocused(window.hasFocus());
    }

    while (const std::optional<sf::Event> event = window.pollEvent()) {
        if (!processNativeEvent(window, *event)) {
            break;
        }
    }
    processPlatformScrollEvents(window);

    if (!eventPump_.useInjectedMouseOnly_ && window.hasFocus()) {
        const sf::Vector2i pixel = sf::Mouse::getPosition(window);
        updatePointerViewportState(pixel);
        const sf::Vector2i polled = pixelToWorld(window, pixel);
        if (acceptsPointerPixel(pixel) || !pointer_.mouseTriggers_.empty()) {
            if (polled != pointer_.mousePosition_) {
                const sf::Vector2i previous = pointer_.mousePosition_;
                pointer_.mousePosition_ = polled;
                pointer_.mouseMoved_ = true;
                pointer_.mouseMovedDelta_ = polled - previous;
            }
        } else {
            pointer_.mousePosition_ = polled;
        }
    }

    if (keyboard_.enteredText_.size() == 1 &&
        keyboard_.enteredText_[0] == '\x16') {
        const sf::U8String clipboard = sf::Clipboard::getString().toUtf8();
        keyboard_.enteredText_.assign(
            reinterpret_cast<const char*>(clipboard.data()), clipboard.size());
    }

    updateJoystickDominantAxes();
    updateInputType(window);
    dispatchActionMappings();
    if (frameCompletionCallback_) {
        frameCompletionCallback_();
    }
}

bool InputRuntime::isMouseInputMode() const {
    return eventPump_.currentInputType_ == InputType::Mouse;
}

void InputRuntime::blockInput() {
    keyboard_.blocked_ = true;
    pointer_.mouseBlocked_ = true;
    joystick_.blocked_ = true;
    blockTouch();
}

void InputRuntime::unblockInput() {
    keyboard_.blocked_ = false;
    pointer_.mouseBlocked_ = false;
    joystick_.blocked_ = false;
    pointer_.touchBlocked_ = false;
}

void InputRuntime::setFrameCompletionCallback(std::function<void()> callback) {
    frameCompletionCallback_ = std::move(callback);
}

void InputRuntime::shutdown() noexcept {
    InputEventPump::pendingSystemCancel_.store(false,
                                               std::memory_order_release);
    ludork::engine::platform_input::shutdown();
    actions_.mappings_.clear();
    frameCompletionCallback_ = {};
    {
        const std::lock_guard<std::mutex> lock(eventPump_.injectedEventsMutex_);
        eventPump_.injectedEvents_.clear();
    }
    resetFrameState();
    clearKeyboardState();
    pointer_.mouseTriggers_.clear();
    keyboard_.heldKeys_.clear();
    keyboard_.heldScans_.clear();
    abortTwoFingerCancel();
    pointer_.primaryTouchFinger_.reset();
    pointer_.touchFingers_.clear();
    pointer_.touchActive_ = false;
    pointer_.touchPosition_.reset();
    pointer_.touchTravelDistance_ = 0.0f;
    pointer_.touchDragged_ = false;
    pointer_.touchGestureSuppressed_ = false;
    pointer_.touchTrigger_ = {};
    pointer_.touchBlocked_ = false;
    pointer_.touchTapPosition_.reset();
    pointer_.viewport_.reset();
    pointer_.insideViewport_ = true;
    pointer_.injectedPixel_.reset();
    pointer_.injectedTransitionPending_.reset();
    joystick_.axisStatus_.clear();
    joystick_.dominantAxis_.clear();
    joystick_.axisTriggers_.clear();
    joystick_.buttonTriggers_.clear();
    eventPump_.activeWindow_ = nullptr;
}

InputRuntime& inputRuntime() {
    static InputRuntime runtime;
    return runtime;
}
