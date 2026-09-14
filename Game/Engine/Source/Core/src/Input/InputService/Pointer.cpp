#include "InputImpl.hpp"

#include "Platform/PlatformInputBridge.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

namespace ludork::engine::input_impl {

sf::Vector2i InputImpl::pixelToWorld(sf::RenderWindow& window,
                                     const sf::Vector2i& pixel) {
    const sf::Vector2f world = window.mapPixelToCoords(pixel);
    return {static_cast<int>(world.x), static_cast<int>(world.y)};
}

sf::Vector2i InputImpl::worldToPixel(sf::RenderWindow& window,
                                     const sf::Vector2i& position) {
    return window.mapCoordsToPixel(
        {static_cast<float>(position.x), static_cast<float>(position.y)});
}

void InputImpl::setMouseButtonPressed(sf::Mouse::Button button,
                                      const sf::Vector2i& position) {
    const int buttonCode = static_cast<int>(button);
    pointer_.mouseButtonPressed_ = true;
    pointer_.pendingMouseTriggerReleases_.erase(buttonCode);
    pointer_.mousePressedEvents_[buttonCode] = true;
    pointer_.mousePosition_ = position;
    InputTriggerEntry& entry = pointer_.mouseTriggers_[buttonCode];
    ++entry.count;
}

void InputImpl::setMouseButtonReleased(sf::Mouse::Button button,
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

void InputImpl::beginTwoFingerCancel(const sf::Vector2i& position) {
    if (pointer_.touchCancelMouseActive_) {
        return;
    }
    cancelTouchGesture();
    setMouseButtonPressed(sf::Mouse::Button::Right, position);
    pointer_.touchCancelMouseActive_ = true;
    pointer_.touchCancelMousePressedThisFrame_ = true;
}

void InputImpl::endTwoFingerCancel(const sf::Vector2i& position) {
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

void InputImpl::releasePendingTwoFingerCancel() {
    if (!pointer_.touchCancelMouseReleasePending_.has_value()) {
        return;
    }
    const sf::Vector2i position = *pointer_.touchCancelMouseReleasePending_;
    pointer_.touchCancelMouseReleasePending_.reset();
    setMouseButtonReleased(sf::Mouse::Button::Right, position);
}

void InputImpl::abortTwoFingerCancel() {
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

bool InputImpl::acceptsPointerPixel(const sf::Vector2i& pixel) const {
    return !pointer_.viewport_.has_value() ||
           pointer_.viewport_->contains(pixel);
}

void InputImpl::updatePointerViewportState(bool inside) {
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

void InputImpl::updatePointerViewportState(const sf::Vector2i& pixel) {
    updatePointerViewportState(acceptsPointerPixel(pixel));
}

void InputImpl::recordMouseWheel(sf::Mouse::Wheel wheel, float delta,
                                 const sf::Vector2i& position, bool precise) {
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

bool InputImpl::isMouseWheelScrolled() const {
    return pointer_.mouseWheelScrolled_ && !isMouseBlocked();
}

std::optional<sf::Mouse::Wheel> InputImpl::getMouseScrolledWheel() const {
    return isMouseWheelScrolled() ? pointer_.mouseWheel_
                                  : std::optional<sf::Mouse::Wheel>{};
}

float InputImpl::getMouseScrolledWheelDelta() const {
    return isMouseWheelScrolled() ? pointer_.mouseWheelDelta_ : 0.0f;
}

bool InputImpl::isMouseWheelPrecise() const {
    return isMouseWheelScrolled() && pointer_.mouseWheelPrecise_;
}

std::optional<sf::Vector2i> InputImpl::getMouseScrolledWheelPosition() const {
    return isMouseWheelScrolled() ? pointer_.mouseWheelPosition_
                                  : std::optional<sf::Vector2i>{};
}

bool InputImpl::isMouseButtonPressed() const {
    return pointer_.mouseButtonPressed_ && !isMouseBlocked();
}

bool InputImpl::isMouseButtonReleased() const {
    return pointer_.mouseButtonReleased_ && !isMouseBlocked();
}

bool InputImpl::getMouseButtonPressed(sf::Mouse::Button button, bool handled) {
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

bool InputImpl::getMouseButtonReleased(sf::Mouse::Button button, bool handled) {
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

bool InputImpl::isMouseMoved() const {
    return pointer_.mouseMoved_ && !isMouseBlocked();
}

sf::Vector2i InputImpl::getMousePosition() const {
    return pointer_.mousePosition_;
}

std::optional<sf::Vector2i> InputImpl::getMouseMovedDelta() const {
    return isMouseMoved() ? pointer_.mouseMovedDelta_
                          : std::optional<sf::Vector2i>{};
}

void InputImpl::setMousePosition(const sf::Vector2i& position) {
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

void InputImpl::setMousePosition(const sf::Vector2i& position,
                                 sf::WindowBase& window) {
    if (isInputCaptured()) {
        return;
    }
    if (!ludork::engine::platform_input::setMousePosition(window, position)) {
        sf::Mouse::setPosition(position, window);
    }
}

bool InputImpl::isMouseEntered() const {
    return pointer_.mouseEntered_ && !isMouseBlocked();
}

bool InputImpl::isMouseLeft() const {
    return pointer_.mouseLeft_ && !isMouseBlocked();
}

bool InputImpl::isTouchBegan(bool handled) {
    if (isTouchBlocked() || pointer_.touchGestureSuppressed_ ||
        !pointer_.touchBegan_ || pointer_.touchBeganHandled_) {
        return false;
    }
    if (handled) {
        pointer_.touchBeganHandled_ = true;
    }
    return true;
}

bool InputImpl::isTouchTap(bool handled) {
    if (isTouchBlocked() || pointer_.touchGestureSuppressed_ ||
        !pointer_.touchTap_ || pointer_.touchTapHandled_) {
        return false;
    }
    if (handled) {
        pointer_.touchTapHandled_ = true;
    }
    return true;
}

bool InputImpl::isTouchEnded() const {
    return pointer_.touchEnded_ && !isTouchBlocked() &&
           !pointer_.touchGestureSuppressed_;
}

bool InputImpl::isTouchMoved() const {
    return pointer_.touchMoved_ && !isTouchBlocked() &&
           !pointer_.touchGestureSuppressed_;
}

bool InputImpl::isTouchDragged() const {
    return pointer_.touchDragged_ && !isTouchBlocked() &&
           !pointer_.touchGestureSuppressed_;
}

bool InputImpl::isTouchActive() const {
    return pointer_.touchActive_ && !isTouchBlocked();
}

std::optional<sf::Vector2i> InputImpl::getTouchPosition() const {
    return isInputCaptured() ? std::nullopt : pointer_.touchPosition_;
}

std::optional<sf::Vector2i> InputImpl::getTouchBeganPosition() const {
    return isInputCaptured() ? std::nullopt : pointer_.touchBeganPosition_;
}

std::optional<sf::Vector2i> InputImpl::getTouchTapPosition() const {
    return isInputCaptured() ? std::nullopt : pointer_.touchTapPosition_;
}

std::optional<sf::Vector2i> InputImpl::getTouchEndedPosition() const {
    return isInputCaptured() ? std::nullopt : pointer_.touchEndedPosition_;
}

std::optional<sf::Vector2i> InputImpl::getTouchMovedDelta() const {
    return isTouchMoved() ? pointer_.touchMovedDelta_
                          : std::optional<sf::Vector2i>{};
}

void InputImpl::cancelTouchGesture() noexcept {
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

bool InputImpl::isTouchTriggered(bool handled) {
    if (isTouchBlocked() || pointer_.touchDragged_ ||
        pointer_.touchGestureSuppressed_ || pointer_.touchTrigger_.handled ||
        pointer_.touchTrigger_.count < 1) {
        return false;
    }
    if (handled) {
        pointer_.touchTrigger_.handled = true;
    }
    return true;
}

bool InputImpl::isTouchBlocked() const {
    return isInputCaptured() || pointer_.touchBlocked_;
}

void InputImpl::blockTouch() {
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

void InputImpl::unblockTouch() {
    pointer_.touchBlocked_ = false;
}

bool InputImpl::isMouseButtonTriggered(sf::Mouse::Button button, bool handled) {
    if (isMouseBlocked()) {
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

bool InputImpl::isMouseButtonDown(sf::Mouse::Button button) const {
    if (isMouseBlocked() || !modal_.allowsMouseButton(button)) {
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

bool InputImpl::isMouseBlocked() const {
    return isInputCaptured() || pointer_.mouseBlocked_;
}

void InputImpl::blockMouse() {
    pointer_.mouseBlocked_ = true;
}

void InputImpl::unblockMouse() {
    pointer_.mouseBlocked_ = false;
}

}  // namespace ludork::engine::input_impl
