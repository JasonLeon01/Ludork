#include <UI/FunctionalBase.hpp>

#include "FunctionalBase/InteractionRuntime.hpp"

#include <Runtime/EngineState.hpp>
#include <UI/ControlBase.hpp>

#include <array>

FunctionalInputProvider* FunctionalBase::inputProvider_ = nullptr;
FunctionalBase::FocusResolver FunctionalBase::keyboardFocusResolver_;
FunctionalBase::DirectionalFocusRequester
    FunctionalBase::directionalFocusRequester_;
FunctionalBase::FocusSetter FunctionalBase::keyboardFocusSetter_;
FunctionalBase::FocusResolver FunctionalBase::keyboardCursorResolver_;

FunctionalBase::~FunctionalBase() {
    if (pointerSource_ == PointerSource::Touch && inputProvider_ != nullptr) {
        inputProvider_->cancelTouchGesture();
    }
}

void FunctionalBase::setInputProvider(FunctionalInputProvider* provider) {
    inputProvider_ = provider;
}

void FunctionalBase::setKeyboardFocusResolver(FocusResolver resolver) {
    keyboardFocusResolver_ = std::move(resolver);
}

void FunctionalBase::setDirectionalFocusRequester(
    DirectionalFocusRequester requester) {
    directionalFocusRequester_ = std::move(requester);
}

void FunctionalBase::setKeyboardFocusSetter(FocusSetter setter) {
    keyboardFocusSetter_ = std::move(setter);
}

void FunctionalBase::setKeyboardCursorResolver(FocusResolver resolver) {
    keyboardCursorResolver_ = std::move(resolver);
}

void FunctionalBase::resetRuntimeCallbacks() noexcept {
    inputProvider_ = nullptr;
    keyboardFocusResolver_ = {};
    directionalFocusRequester_ = {};
    keyboardFocusSetter_ = {};
    keyboardCursorResolver_ = {};
}

bool FunctionalBase::canReceiveFocus() const {
    return getCanReceiveFocus() && isInteractionEnabled();
}

bool FunctionalBase::shouldDispatchKeyboardInput() const {
    return isInteractionEnabled() &&
           (!keyboardFocusResolver_ || keyboardFocusResolver_(*this));
}

bool FunctionalBase::requestDirectionalFocusMove(const std::string& direction) {
    return isInteractionEnabled() && directionalFocusRequester_ &&
           directionalFocusRequester_(*this, direction);
}

bool FunctionalBase::requestKeyboardFocus() {
    return isInteractionEnabled() && keyboardFocusSetter_ &&
           keyboardFocusSetter_(*this);
}

bool FunctionalBase::ownsKeyboardCursorFocus() const {
    return isInteractionEnabled() &&
           (keyboardCursorResolver_ ? keyboardCursorResolver_(*this)
                                    : getFocused());
}

bool FunctionalBase::isHovered() const {
    return hovered_;
}

bool FunctionalBase::isPressed() const {
    return pressed_;
}

bool FunctionalBase::getActive() const {
    return active_;
}

void FunctionalBase::setActive(bool active) {
    if (active_ == active) {
        return;
    }
    active_ = active;
    if (!active_) {
        const bool hadPointerInteraction =
            hovered_ || pressed_ || pointerSource_ != PointerSource::None;
        ControlBase* control = dynamic_cast<ControlBase*>(this);
        if (control != nullptr) {
            ControlBase::resetFunctionalInteractions(*control);
        } else {
            resetPointerInteraction();
        }
        if (!hadPointerInteraction) {
            onInteractionStateChanged();
        }
        return;
    }
    onInteractionStateChanged();
}

void FunctionalBase::setTouchHitBounds(
    const std::optional<sf::FloatRect>& bounds) {
    ludork::engine::functional_base_impl::validateTouchHitBounds(bounds);
    touchHitBounds_ = bounds;
}

std::optional<sf::FloatRect> FunctionalBase::getAbsoluteTouchHitBounds() const {
    const ControlBase* control = dynamic_cast<const ControlBase*>(this);
    if (control == nullptr) {
        return std::nullopt;
    }
    if (!touchHitBounds_.has_value()) {
        return control->getAbsoluteBounds();
    }
    const sf::FloatRect scaledBounds(touchHitBounds_->position * Scale,
                                     touchHitBounds_->size * Scale);
    return control->screenRenderTransform().transformRect(scaledBounds);
}

void FunctionalBase::addConfirmCallback(EventCallback callback) {
    confirmCallback_ = std::move(callback);
}

void FunctionalBase::addCancelCallback(EventCallback callback) {
    cancelCallback_ = std::move(callback);
}

void FunctionalBase::addClickCallback(EventCallback callback) {
    clickCallback_ = std::move(callback);
}

void FunctionalBase::addMouseButtonDownCallback(HandledEventCallback callback) {
    mouseButtonDownCallback_ = std::move(callback);
}

void FunctionalBase::addHoverCallback(EventCallback callback) {
    hoverCallback_ = std::move(callback);
}

void FunctionalBase::addUnHoverCallback(EventCallback callback) {
    unHoverCallback_ = std::move(callback);
}

void FunctionalBase::addMouseMovedCallback(EventCallback callback) {
    mouseMovedCallback_ = std::move(callback);
}

void FunctionalBase::addMouseWheelScrolledCallback(EventCallback callback) {
    mouseWheelScrolledCallback_ = std::move(callback);
}

void FunctionalBase::addKeyDownCallback(EventCallback callback) {
    keyDownCallback_ = std::move(callback);
}

void FunctionalBase::addKeyUpCallback(EventCallback callback) {
    keyUpCallback_ = std::move(callback);
}

void FunctionalBase::clearEventCallbacks() noexcept {
    confirmCallback_ = {};
    cancelCallback_ = {};
    clickCallback_ = {};
    mouseButtonDownCallback_ = {};
    hoverCallback_ = {};
    unHoverCallback_ = {};
    mouseMovedCallback_ = {};
    mouseWheelScrolledCallback_ = {};
    keyDownCallback_ = {};
    keyUpCallback_ = {};
}

void FunctionalBase::update(float deltaTime) {
    onTick(deltaTime);
    if (inputProvider_ == nullptr) {
        resetPointerInteraction();
        return;
    }
    ControlBase* control = dynamic_cast<ControlBase*>(this);
    const sf::Vector2f mousePosition(inputProvider_->getMousePosition());
    if (!isInteractionEnabled()) {
        resetPointerInteraction();
        return;
    }

    if (control != nullptr && pointerSource_ == PointerSource::None &&
        acceptsTouchCapture()) {
        const std::optional<sf::FloatRect> touchBounds =
            getAbsoluteTouchHitBounds();
        if (touchBounds.has_value() && inputProvider_->isTouchBegan(false)) {
            const std::optional<sf::Vector2i> beganPosition =
                inputProvider_->getTouchBeganPosition();
            if (beganPosition.has_value() &&
                touchBounds->contains(sf::Vector2f(*beganPosition))) {
                beginTouchPress();
                inputProvider_->isTouchBegan(true);
                onTouchCaptureBegan(sf::Vector2f(*beganPosition));
            }
        }
    }
    if (!isInteractionEnabled()) {
        resetPointerInteraction();
        return;
    }

    if (pointerSource_ != PointerSource::Touch) {
        static constexpr std::array buttons = {
            sf::Mouse::Button::Left,
            sf::Mouse::Button::Right,
            sf::Mouse::Button::Middle,
        };
        std::array<bool, buttons.size()> mousePressed = {};
        bool mousePressReceived = false;
        if (inputProvider_->isMouseButtonPressed()) {
            for (std::size_t index = 0; index < buttons.size(); ++index) {
                if (!isInteractionEnabled()) {
                    break;
                }
                const sf::Mouse::Button button = buttons[index];
                mousePressed[index] =
                    inputProvider_->getMouseButtonPressed(button, false);
                mousePressReceived = mousePressReceived || mousePressed[index];
                if (mousePressed[index] &&
                    onMouseButtonDown(
                        mouseButtonArguments(mousePosition, button))) {
                    inputProvider_->getMouseButtonPressed(button, true);
                    inputProvider_->isMouseButtonTriggered(button, true);
                }
            }
        }
        if (!isInteractionEnabled()) {
            resetPointerInteraction();
            return;
        }

        bool hovered = control != nullptr &&
                       control->getAbsoluteBounds().contains(mousePosition);
        if (!inputProvider_->isMouseInputMode()) {
            hovered = false;
        }
        setHovered(hovered, mousePosition);
        if (!isInteractionEnabled()) {
            resetPointerInteraction();
            return;
        }
        if (hovered) {
            if (pointerSource_ == PointerSource::None) {
                for (std::size_t index = 0; index < buttons.size(); ++index) {
                    if (mousePressed[index]) {
                        beginMousePress(buttons[index]);
                        break;
                    }
                }
            }
            if (!isInteractionEnabled()) {
                resetPointerInteraction();
                return;
            }
            if (inputProvider_->isMouseMoved()) {
                onMouseMoved(pointerArguments(mousePosition));
            }
            if (!isInteractionEnabled()) {
                resetPointerInteraction();
                return;
            }
            if (mousePressReceived) {
                onClick(pointerArguments(mousePosition));
            }
            if (!isInteractionEnabled()) {
                resetPointerInteraction();
                return;
            }
            if (inputProvider_->isMouseWheelScrolled()) {
                onMouseWheelScrolled(mouseWheelArguments(
                    mousePosition,
                    inputProvider_->getMouseScrolledWheelDelta()));
            }
            if (!isInteractionEnabled()) {
                resetPointerInteraction();
                return;
            }
        }

        if (pointerSource_ == PointerSource::Mouse) {
            const sf::Mouse::Button button = *pressedMouseButton_;
            const bool released =
                inputProvider_->isMouseButtonReleased() &&
                inputProvider_->getMouseButtonReleased(button, false);
            if (!isInteractionEnabled() || !hovered || released ||
                !inputProvider_->isMouseButtonDown(button)) {
                endPointerPress();
            }
        }
    } else {
        setHovered(false, mousePosition);
    }
    if (!isInteractionEnabled()) {
        resetPointerInteraction();
        return;
    }

    if (control != nullptr) {
        if (pointerSource_ == PointerSource::Touch) {
            const bool ended = inputProvider_->isTouchEnded();
            const std::optional<sf::Vector2i> position =
                ended ? inputProvider_->getTouchEndedPosition()
                      : inputProvider_->getTouchPosition();
            if (inputProvider_->isTouchMoved() && position.has_value()) {
                onMouseMoved(pointerArguments(sf::Vector2f(*position)));
            }
            if (!isInteractionEnabled()) {
                resetPointerInteraction();
                return;
            }
            if (ended) {
                if (inputProvider_->isTouchTap(false)) {
                    const std::optional<sf::FloatRect> releaseBounds =
                        getAbsoluteTouchHitBounds();
                    if (position.has_value() && releaseBounds.has_value() &&
                        releaseBounds->contains(sf::Vector2f(*position))) {
                        const RuntimeValue::Map arguments =
                            pointerArguments(sf::Vector2f(*position));
                        if (confirmCallback_) {
                            onConfirm(arguments);
                        } else {
                            onClick(arguments);
                        }
                    }
                    inputProvider_->isTouchTap(true);
                }
                endPointerPress();
            } else if (!isInteractionEnabled() ||
                       !inputProvider_->isTouchActive() ||
                       !position.has_value()) {
                resetPointerInteraction();
            }
        }
    } else if (pointerSource_ == PointerSource::Touch) {
        resetPointerInteraction();
    }

    if (shouldDispatchKeyboardInput()) {
        if (inputProvider_->isKeyPressed() ||
            inputProvider_->isJoystickButtonPressed() ||
            inputProvider_->isJoystickAxisMoved()) {
            onKeyDown({});
        }
        if (shouldDispatchKeyboardInput() &&
            (inputProvider_->isKeyReleased() ||
             inputProvider_->isJoystickButtonReleased())) {
            onKeyUp({});
        }
    }
}

void FunctionalBase::lateUpdate(float deltaTime) {
    onLateTick(deltaTime);
}

void FunctionalBase::fixedUpdate(float fixedDelta) {
    onFixedTick(fixedDelta);
}

void FunctionalBase::onConfirm(const RuntimeValue::Map& arguments) {
    if (confirmCallback_) {
        confirmCallback_(*this, arguments);
    }
}

void FunctionalBase::onCancel(const RuntimeValue::Map& arguments) {
    if (cancelCallback_) {
        cancelCallback_(*this, arguments);
    }
}

void FunctionalBase::onClick(const RuntimeValue::Map& arguments) {
    if (clickCallback_) {
        clickCallback_(*this, arguments);
    }
}

bool FunctionalBase::onMouseButtonDown(const RuntimeValue::Map& arguments) {
    return mouseButtonDownCallback_ &&
           mouseButtonDownCallback_(*this, arguments);
}

void FunctionalBase::onHover(const RuntimeValue::Map& arguments) {
    if (hoverCallback_) {
        hoverCallback_(*this, arguments);
    }
}

void FunctionalBase::onUnHover(const RuntimeValue::Map& arguments) {
    if (unHoverCallback_) {
        unHoverCallback_(*this, arguments);
    }
}

void FunctionalBase::onMouseMoved(const RuntimeValue::Map& arguments) {
    if (mouseMovedCallback_) {
        mouseMovedCallback_(*this, arguments);
    }
}

void FunctionalBase::onMouseWheelScrolled(const RuntimeValue::Map& arguments) {
    if (mouseWheelScrolledCallback_) {
        mouseWheelScrolledCallback_(*this, arguments);
    }
}

void FunctionalBase::onKeyDown(const RuntimeValue::Map& arguments) {
    if (keyDownCallback_) {
        keyDownCallback_(*this, arguments);
    }
}

void FunctionalBase::onKeyUp(const RuntimeValue::Map& arguments) {
    if (keyUpCallback_) {
        keyUpCallback_(*this, arguments);
    }
}

void FunctionalBase::onTick(float) {}

void FunctionalBase::onLateTick(float) {}

void FunctionalBase::onFixedTick(float) {}

FunctionalInputProvider* FunctionalBase::inputProvider() {
    return inputProvider_;
}

bool FunctionalBase::isInteractionEnabled() const {
    if (!active_) {
        return false;
    }
    const ControlBase* control = dynamic_cast<const ControlBase*>(this);
    if (control == nullptr) {
        return true;
    }
    if (!control->getVisible()) {
        return false;
    }
    std::shared_ptr<ControlBase> parent = control->getParent();
    while (parent != nullptr) {
        if (!parent->getVisible()) {
            return false;
        }
        const FunctionalBase* functional =
            dynamic_cast<const FunctionalBase*>(parent.get());
        if (functional != nullptr && !functional->getActive()) {
            return false;
        }
        parent = parent->getParent();
    }
    return true;
}

bool FunctionalBase::acceptsTouchCapture() const {
    return getCanReceiveFocus() || static_cast<bool>(confirmCallback_) ||
           static_cast<bool>(clickCallback_);
}

void FunctionalBase::onTouchCaptureBegan(const sf::Vector2f&) {}

bool FunctionalBase::hasTouchCapture() const {
    return pointerSource_ == PointerSource::Touch;
}

void FunctionalBase::onPointerInteractionReset() {}

void FunctionalBase::resetPointerInteraction() {
    if (ludork::engine::functional_base_impl::pointerStateIsEmpty(
            hovered_, pressed_, pointerSource_ != PointerSource::None)) {
        onPointerInteractionReset();
        return;
    }
    if (pointerSource_ == PointerSource::Touch && inputProvider_ != nullptr) {
        inputProvider_->cancelTouchGesture();
    }
    hovered_ = false;
    pressed_ = false;
    pointerSource_ = PointerSource::None;
    pressedMouseButton_.reset();
    onPointerInteractionReset();
    onInteractionStateChanged();
}

void FunctionalBase::onInteractionStateChanged() {}

RuntimeValue::Map FunctionalBase::pointerArguments(
    const sf::Vector2f& position) {
    return ludork::engine::functional_base_impl::pointerArguments(position);
}

RuntimeValue::Map FunctionalBase::mouseButtonArguments(
    const sf::Vector2f& position, sf::Mouse::Button button) {
    return ludork::engine::functional_base_impl::mouseButtonArguments(position,
                                                                      button);
}

RuntimeValue::Map FunctionalBase::mouseWheelArguments(
    const sf::Vector2f& position, float delta) {
    return ludork::engine::functional_base_impl::mouseWheelArguments(position,
                                                                     delta);
}

void FunctionalBase::setHovered(bool hovered, const sf::Vector2f& position) {
    if (hovered_ == hovered) {
        return;
    }
    hovered_ = hovered;
    onInteractionStateChanged();
    if (hovered_) {
        onHover(pointerArguments(position));
    } else {
        onUnHover(pointerArguments(position));
    }
}

void FunctionalBase::beginMousePress(sf::Mouse::Button button) {
    pressed_ = true;
    pointerSource_ = PointerSource::Mouse;
    pressedMouseButton_ = button;
    onInteractionStateChanged();
}

void FunctionalBase::beginTouchPress() {
    pressed_ = true;
    pointerSource_ = PointerSource::Touch;
    pressedMouseButton_.reset();
    onInteractionStateChanged();
}

void FunctionalBase::endPointerPress() {
    if (!pressed_) {
        return;
    }
    pressed_ = false;
    pointerSource_ = PointerSource::None;
    pressedMouseButton_.reset();
    onInteractionStateChanged();
}
