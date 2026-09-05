#include "InputRuntime.hpp"

#include "Platform/PlatformInputBridge.hpp"

#include <SFML/Graphics/RenderTarget.hpp>

sf::Vector2i InputRuntime::pixelToWorld(sf::WindowBase& window,
                                        const sf::Vector2i& pixel) {
    const sf::RenderTarget* target =
        dynamic_cast<const sf::RenderTarget*>(&window);
    if (target == nullptr) {
        return pixel;
    }
    const sf::Vector2f world = target->mapPixelToCoords(pixel);
    return {static_cast<int>(world.x), static_cast<int>(world.y)};
}

sf::Vector2i InputRuntime::worldToPixel(sf::WindowBase& window,
                                        const sf::Vector2i& position) {
    const sf::RenderTarget* target =
        dynamic_cast<const sf::RenderTarget*>(&window);
    if (target == nullptr) {
        return position;
    }
    return target->mapCoordsToPixel(
        {static_cast<float>(position.x), static_cast<float>(position.y)});
}

void InputRuntime::setMouseButtonPressed(sf::Mouse::Button button,
                                         const sf::Vector2i& position) {
    const int buttonCode = static_cast<int>(button);
    pointer_.mouseButtonPressed_ = true;
    pointer_.pendingMouseTriggerReleases_.erase(buttonCode);
    pointer_.mousePressedEvents_[buttonCode] = true;
    pointer_.mousePosition_ = position;
    InputTriggerEntry& entry = pointer_.mouseTriggers_[buttonCode];
    ++entry.count;
}

void InputRuntime::setMouseButtonReleased(sf::Mouse::Button button,
                                          const sf::Vector2i& position) {
    const int buttonCode = static_cast<int>(button);
    pointer_.mouseButtonReleased_ = true;
    pointer_.mouseReleasedEvents_[buttonCode] = true;
    pointer_.mousePosition_ = position;
    if (pointer_.mousePressedEvents_.contains(buttonCode)) {
        pointer_.pendingMouseTriggerReleases_.insert(buttonCode);
    } else {
        pointer_.mouseTriggers_.erase(buttonCode);
        pointer_.pendingMouseTriggerReleases_.erase(buttonCode);
    }
}

void InputRuntime::beginTwoFingerCancel(const sf::Vector2i& position) {
    if (pointer_.touchCancelMouseActive_) {
        return;
    }
    cancelTouchGesture();
    setMouseButtonPressed(sf::Mouse::Button::Right, position);
    pointer_.touchCancelMouseActive_ = true;
    pointer_.touchCancelMousePressedThisFrame_ = true;
}

void InputRuntime::endTwoFingerCancel(const sf::Vector2i& position) {
    if (!pointer_.touchCancelMouseActive_) {
        return;
    }
    pointer_.touchCancelMouseActive_ = false;
    if (pointer_.touchCancelMousePressedThisFrame_) {
        pointer_.touchCancelMouseReleasePending_ = position;
        return;
    }
    setMouseButtonReleased(sf::Mouse::Button::Right, position);
}

void InputRuntime::releasePendingTwoFingerCancel() {
    if (!pointer_.touchCancelMouseReleasePending_.has_value()) {
        return;
    }
    const sf::Vector2i position = *pointer_.touchCancelMouseReleasePending_;
    pointer_.touchCancelMouseReleasePending_.reset();
    setMouseButtonReleased(sf::Mouse::Button::Right, position);
}

void InputRuntime::abortTwoFingerCancel() {
    const bool syntheticMouseActive =
        pointer_.touchCancelMouseActive_ ||
        pointer_.touchCancelMouseReleasePending_.has_value();
    const int button = static_cast<int>(sf::Mouse::Button::Right);
    pointer_.touchCancelMouseActive_ = false;
    pointer_.touchCancelMousePressedThisFrame_ = false;
    pointer_.touchCancelMouseReleasePending_.reset();
    if (!syntheticMouseActive) {
        return;
    }
    pointer_.mouseTriggers_.erase(button);
    pointer_.pendingMouseTriggerReleases_.erase(button);
    pointer_.mousePressedEvents_.erase(button);
    pointer_.mouseReleasedEvents_.erase(button);
}

bool InputRuntime::acceptsPointerPixel(const sf::Vector2i& pixel) const {
    return !pointer_.viewport_.has_value() ||
           pointer_.viewport_->contains(pixel);
}

void InputRuntime::updatePointerViewportState(bool inside) {
    if (inside == pointer_.insideViewport_) {
        return;
    }
    pointer_.insideViewport_ = inside;
    if (inside) {
        pointer_.mouseEntered_ = true;
    } else {
        pointer_.mouseLeft_ = true;
    }
}

void InputRuntime::updatePointerViewportState(const sf::Vector2i& pixel) {
    updatePointerViewportState(acceptsPointerPixel(pixel));
}

void InputRuntime::recordMouseWheel(sf::Mouse::Wheel wheel, float delta,
                                    const sf::Vector2i& position,
                                    bool precise) {
    if (delta == 0.0f) {
        return;
    }
    if (pointer_.mouseWheelScrolled_ &&
        pointer_.mouseWheel_ == sf::Mouse::Wheel::Vertical &&
        wheel != sf::Mouse::Wheel::Vertical) {
        return;
    }
    if (!pointer_.mouseWheelScrolled_ || pointer_.mouseWheel_ != wheel ||
        pointer_.mouseWheelPrecise_ != precise) {
        pointer_.mouseWheelDelta_ = 0.0f;
    }
    pointer_.mouseWheelScrolled_ = true;
    pointer_.mouseWheel_ = wheel;
    pointer_.mouseWheelDelta_ += delta;
    pointer_.mouseWheelPrecise_ = precise;
    pointer_.mouseWheelPosition_ = position;
}

bool InputRuntime::isMouseWheelScrolled() const {
    return pointer_.mouseWheelScrolled_ && !pointer_.mouseBlocked_;
}

std::optional<sf::Mouse::Wheel> InputRuntime::getMouseScrolledWheel() const {
    return isMouseWheelScrolled() ? pointer_.mouseWheel_
                                  : std::optional<sf::Mouse::Wheel>{};
}

float InputRuntime::getMouseScrolledWheelDelta() const {
    return isMouseWheelScrolled() ? pointer_.mouseWheelDelta_ : 0.0f;
}

bool InputRuntime::isMouseWheelPrecise() const {
    return isMouseWheelScrolled() && pointer_.mouseWheelPrecise_;
}

std::optional<sf::Vector2i> InputRuntime::getMouseScrolledWheelPosition()
    const {
    return isMouseWheelScrolled() ? pointer_.mouseWheelPosition_
                                  : std::optional<sf::Vector2i>{};
}

bool InputRuntime::isMouseButtonPressed() const {
    return pointer_.mouseButtonPressed_ && !pointer_.mouseBlocked_;
}

bool InputRuntime::isMouseButtonReleased() const {
    return pointer_.mouseButtonReleased_ && !pointer_.mouseBlocked_;
}

bool InputRuntime::getMouseButtonPressed(sf::Mouse::Button button,
                                         bool handled) {
    if (!isMouseButtonPressed()) {
        return false;
    }
    const auto iterator =
        pointer_.mousePressedEvents_.find(static_cast<int>(button));
    if (iterator == pointer_.mousePressedEvents_.end()) {
        return false;
    }
    const bool result = iterator->second;
    if (result && handled) {
        iterator->second = false;
    }
    return result;
}

bool InputRuntime::getMouseButtonReleased(sf::Mouse::Button button,
                                          bool handled) {
    if (!isMouseButtonReleased()) {
        return false;
    }
    const auto iterator =
        pointer_.mouseReleasedEvents_.find(static_cast<int>(button));
    if (iterator == pointer_.mouseReleasedEvents_.end()) {
        return false;
    }
    const bool result = iterator->second;
    if (result && handled) {
        iterator->second = false;
    }
    return result;
}

bool InputRuntime::isMouseMoved() const {
    return pointer_.mouseMoved_ && !pointer_.mouseBlocked_;
}

sf::Vector2i InputRuntime::getMousePosition() const {
    return pointer_.mousePosition_;
}

std::optional<sf::Vector2i> InputRuntime::getMouseMovedDelta() const {
    return isMouseMoved() ? pointer_.mouseMovedDelta_
                          : std::optional<sf::Vector2i>{};
}

void InputRuntime::setMousePosition(const sf::Vector2i& position) {
    if (eventPump_.activeWindow_ == nullptr ||
        !eventPump_.activeWindow_->isOpen()) {
        return;
    }
    const sf::Vector2i pixel =
        worldToPixel(*eventPump_.activeWindow_, position);
    setMousePosition(pixel, *eventPump_.activeWindow_);
    const sf::Vector2i actualPixel =
        sf::Mouse::getPosition(*eventPump_.activeWindow_);
    updatePointerViewportState(actualPixel);
    if (!acceptsPointerPixel(actualPixel) && pointer_.mouseTriggers_.empty()) {
        pointer_.mousePosition_ =
            pixelToWorld(*eventPump_.activeWindow_, actualPixel);
        return;
    }
    const sf::Vector2i synchronizedPosition =
        pixelToWorld(*eventPump_.activeWindow_, actualPixel);
    if (synchronizedPosition != pointer_.mousePosition_) {
        const sf::Vector2i previous = pointer_.mousePosition_;
        pointer_.mousePosition_ = synchronizedPosition;
        pointer_.mouseMoved_ = true;
        pointer_.mouseMovedDelta_ = synchronizedPosition - previous;
    }
}

void InputRuntime::setMousePosition(const sf::Vector2i& position,
                                    sf::WindowBase& window) {
    if (!ludork::engine::platform_input::setMousePosition(window, position)) {
        sf::Mouse::setPosition(position, window);
    }
}

bool InputRuntime::isMouseEntered() const {
    return pointer_.mouseEntered_ && !pointer_.mouseBlocked_;
}

bool InputRuntime::isMouseLeft() const {
    return pointer_.mouseLeft_ && !pointer_.mouseBlocked_;
}

bool InputRuntime::isTouchBegan(bool handled) {
    if (pointer_.touchBlocked_ || pointer_.touchGestureSuppressed_ ||
        !pointer_.touchBegan_ || pointer_.touchBeganHandled_) {
        return false;
    }
    if (handled) {
        pointer_.touchBeganHandled_ = true;
    }
    return true;
}

bool InputRuntime::isTouchTap(bool handled) {
    if (pointer_.touchBlocked_ || pointer_.touchGestureSuppressed_ ||
        !pointer_.touchTap_ || pointer_.touchTapHandled_) {
        return false;
    }
    if (handled) {
        pointer_.touchTapHandled_ = true;
    }
    return true;
}

bool InputRuntime::isTouchEnded() const {
    return pointer_.touchEnded_ && !pointer_.touchBlocked_ &&
           !pointer_.touchGestureSuppressed_;
}

bool InputRuntime::isTouchMoved() const {
    return pointer_.touchMoved_ && !pointer_.touchBlocked_ &&
           !pointer_.touchGestureSuppressed_;
}

bool InputRuntime::isTouchDragged() const {
    return pointer_.touchDragged_ && !pointer_.touchBlocked_ &&
           !pointer_.touchGestureSuppressed_;
}

bool InputRuntime::isTouchActive() const {
    return pointer_.touchActive_ && !pointer_.touchBlocked_;
}

std::optional<sf::Vector2i> InputRuntime::getTouchPosition() const {
    return pointer_.touchPosition_;
}

std::optional<sf::Vector2i> InputRuntime::getTouchBeganPosition() const {
    return pointer_.touchBeganPosition_;
}

std::optional<sf::Vector2i> InputRuntime::getTouchTapPosition() const {
    return pointer_.touchTapPosition_;
}

std::optional<sf::Vector2i> InputRuntime::getTouchEndedPosition() const {
    return pointer_.touchEndedPosition_;
}

std::optional<sf::Vector2i> InputRuntime::getTouchMovedDelta() const {
    return isTouchMoved() ? pointer_.touchMovedDelta_
                          : std::optional<sf::Vector2i>{};
}

void InputRuntime::cancelTouchGesture() noexcept {
    pointer_.touchGestureSuppressed_ = true;
    pointer_.touchActive_ = false;
    pointer_.touchBeganHandled_ = true;
    pointer_.touchMoved_ = false;
    pointer_.touchMovedDelta_.reset();
    pointer_.touchTap_ = false;
    pointer_.touchTapHandled_ = true;
    pointer_.touchTapPosition_.reset();
    pointer_.touchEnded_ = false;
    pointer_.touchEndedPosition_.reset();
    pointer_.touchTrigger_ = {};
}

bool InputRuntime::isTouchTriggered(bool handled) {
    if (pointer_.touchBlocked_ || pointer_.touchDragged_ ||
        pointer_.touchGestureSuppressed_ || pointer_.touchTrigger_.handled ||
        pointer_.touchTrigger_.count < 1) {
        return false;
    }
    if (handled) {
        pointer_.touchTrigger_.handled = true;
    }
    return true;
}

bool InputRuntime::isTouchBlocked() const {
    return pointer_.touchBlocked_;
}

void InputRuntime::blockTouch() {
    abortTwoFingerCancel();
    pointer_.touchTravelDistance_ = 0.0f;
    pointer_.touchDragged_ = false;
    pointer_.touchGestureSuppressed_ = true;
    pointer_.primaryTouchFinger_.reset();
    pointer_.touchFingers_.clear();
    pointer_.touchActive_ = false;
    pointer_.touchTrigger_ = {};
    pointer_.touchBlocked_ = true;
}

void InputRuntime::unblockTouch() {
    pointer_.touchBlocked_ = false;
}

bool InputRuntime::isMouseButtonTriggered(sf::Mouse::Button button,
                                          bool handled) {
    if (pointer_.mouseBlocked_) {
        return false;
    }
    const auto iterator =
        pointer_.mouseTriggers_.find(static_cast<int>(button));
    if (iterator == pointer_.mouseTriggers_.end() || iterator->second.handled ||
        iterator->second.count < 1) {
        return false;
    }
    if (handled) {
        iterator->second.handled = true;
    }
    return true;
}

bool InputRuntime::isMouseButtonDown(sf::Mouse::Button button) const {
    if (pointer_.mouseBlocked_) {
        return false;
    }
    const int buttonCode = static_cast<int>(button);
    if (pointer_.pendingMouseTriggerReleases_.contains(buttonCode)) {
        return false;
    }
    const auto iterator = pointer_.mouseTriggers_.find(buttonCode);
    if (iterator != pointer_.mouseTriggers_.end() &&
        iterator->second.count >= 1) {
        return true;
    }
    if (eventPump_.useInjectedMouseOnly_ ||
        eventPump_.activeWindow_ == nullptr ||
        !eventPump_.activeWindow_->isOpen() ||
        !eventPump_.activeWindow_->hasFocus() || !pointer_.insideViewport_) {
        return false;
    }
    return acceptsPointerPixel(
               sf::Mouse::getPosition(*eventPump_.activeWindow_)) &&
           sf::Mouse::isButtonPressed(button);
}

bool InputRuntime::isMouseBlocked() const {
    return pointer_.mouseBlocked_;
}

void InputRuntime::blockMouse() {
    pointer_.mouseBlocked_ = true;
}

void InputRuntime::unblockMouse() {
    pointer_.mouseBlocked_ = false;
}
