#include <Input/InputService.hpp>
#include <Input/InjectedInputEvent.hpp>
#include <Input/InputActionKey.hpp>
#include <Input/InputNamedValue.hpp>
#include <Input/JoystickAxisEvent.hpp>

#include "InputService/InputImpl.hpp"

#include <UI/FunctionalBase.hpp>

#include <utility>

bool InputService::isInputCaptured() const {
    return ludork::engine::input_impl::inputImpl().isInputCaptured();
}

void InputService::initializeNativePolling() {
    ludork::engine::input_impl::inputImpl().initializeNativePolling();
}

void InputService::update(sf::WindowBase& window) {
    ludork::engine::input_impl::inputImpl().update(window);
}

void InputService::injectEvent(const InjectedInputEvent& event) {
    ludork::engine::input_impl::inputImpl().injectEvent(event);
}

void InputService::setUseInjectedMouseOnly(bool value) {
    ludork::engine::input_impl::inputImpl().setUseInjectedMouseOnly(value);
}

void InputService::setPointerViewport(std::optional<sf::IntRect> viewport) {
    ludork::engine::input_impl::inputImpl().setPointerViewport(
        std::move(viewport));
}

void InputService::onWindowRecreated(sf::WindowBase& window) {
    ludork::engine::input_impl::inputImpl().onWindowRecreated(window);
}

void InputService::requestSystemCancel() noexcept {
    ludork::engine::input_impl::InputEventPumpImpl::requestSystemCancel();
}

bool InputService::isFocused() const {
    return ludork::engine::input_impl::inputImpl().isFocused();
}

bool InputService::isFocusLost() const {
    return ludork::engine::input_impl::inputImpl().isFocusLost();
}

bool InputService::isFocusGained() const {
    return ludork::engine::input_impl::inputImpl().isFocusGained();
}

bool InputService::isKeyPressed() const {
    return ludork::engine::input_impl::inputImpl().isKeyPressed();
}

bool InputService::isKeyReleased() const {
    return ludork::engine::input_impl::inputImpl().isKeyReleased();
}

bool InputService::getKeyPressed(sf::Keyboard::Key key, bool handled, bool alt,
                                 bool ctrl, bool shift, bool system) {
    return ludork::engine::input_impl::inputImpl().getKeyPressed(
        key, handled, alt, ctrl, shift, system);
}

bool InputService::getScanPressed(sf::Keyboard::Scancode scan, bool handled,
                                  bool alt, bool ctrl, bool shift,
                                  bool system) {
    return ludork::engine::input_impl::inputImpl().getScanPressed(
        scan, handled, alt, ctrl, shift, system);
}

bool InputService::getKeyReleased(sf::Keyboard::Key key, bool handled, bool alt,
                                  bool ctrl, bool shift, bool system) {
    return ludork::engine::input_impl::inputImpl().getKeyReleased(
        key, handled, alt, ctrl, shift, system);
}

bool InputService::getScanReleased(sf::Keyboard::Scancode scan, bool handled,
                                   bool alt, bool ctrl, bool shift,
                                   bool system) {
    return ludork::engine::input_impl::inputImpl().getScanReleased(
        scan, handled, alt, ctrl, shift, system);
}

bool InputService::isMouseWheelScrolled() const {
    return ludork::engine::input_impl::inputImpl().isMouseWheelScrolled();
}

std::optional<sf::Mouse::Wheel> InputService::getMouseScrolledWheel() const {
    return ludork::engine::input_impl::inputImpl().getMouseScrolledWheel();
}

float InputService::getMouseScrolledWheelDelta() const {
    return ludork::engine::input_impl::inputImpl().getMouseScrolledWheelDelta();
}

bool InputService::isMouseWheelPrecise() const {
    return ludork::engine::input_impl::inputImpl().isMouseWheelPrecise();
}

std::optional<sf::Vector2i> InputService::getMouseScrolledWheelPosition()
    const {
    return ludork::engine::input_impl::inputImpl()
        .getMouseScrolledWheelPosition();
}

bool InputService::isMouseButtonPressed() const {
    return ludork::engine::input_impl::inputImpl().isMouseButtonPressed();
}

bool InputService::isMouseButtonReleased() const {
    return ludork::engine::input_impl::inputImpl().isMouseButtonReleased();
}

bool InputService::getMouseButtonPressed(sf::Mouse::Button button,
                                         bool handled) {
    return ludork::engine::input_impl::inputImpl().getMouseButtonPressed(
        button, handled);
}

bool InputService::getMouseButtonReleased(sf::Mouse::Button button,
                                          bool handled) {
    return ludork::engine::input_impl::inputImpl().getMouseButtonReleased(
        button, handled);
}

bool InputService::isMouseMoved() const {
    return ludork::engine::input_impl::inputImpl().isMouseMoved();
}

sf::Vector2i InputService::getMousePosition() const {
    return ludork::engine::input_impl::inputImpl().getMousePosition();
}

std::optional<sf::Vector2i> InputService::getMouseMovedDelta() const {
    return ludork::engine::input_impl::inputImpl().getMouseMovedDelta();
}

void InputService::setMousePosition(const sf::Vector2i& position) {
    ludork::engine::input_impl::inputImpl().setMousePosition(position);
}

void InputService::setMousePosition(const sf::Vector2i& position,
                                    sf::WindowBase& window) {
    ludork::engine::input_impl::inputImpl().setMousePosition(position, window);
}

bool InputService::isMouseEntered() const {
    return ludork::engine::input_impl::inputImpl().isMouseEntered();
}

bool InputService::isMouseLeft() const {
    return ludork::engine::input_impl::inputImpl().isMouseLeft();
}

bool InputService::isTouchBegan(bool handled) {
    return ludork::engine::input_impl::inputImpl().isTouchBegan(handled);
}

bool InputService::isTouchTap(bool handled) {
    return ludork::engine::input_impl::inputImpl().isTouchTap(handled);
}

bool InputService::isTouchEnded() const {
    return ludork::engine::input_impl::inputImpl().isTouchEnded();
}

bool InputService::isTouchMoved() const {
    return ludork::engine::input_impl::inputImpl().isTouchMoved();
}

bool InputService::isTouchDragged() const {
    return ludork::engine::input_impl::inputImpl().isTouchDragged();
}

bool InputService::isTouchActive() const {
    return ludork::engine::input_impl::inputImpl().isTouchActive();
}

std::optional<sf::Vector2i> InputService::getTouchPosition() const {
    return ludork::engine::input_impl::inputImpl().getTouchPosition();
}

std::optional<sf::Vector2i> InputService::getTouchBeganPosition() const {
    return ludork::engine::input_impl::inputImpl().getTouchBeganPosition();
}

std::optional<sf::Vector2i> InputService::getTouchTapPosition() const {
    return ludork::engine::input_impl::inputImpl().getTouchTapPosition();
}

std::optional<sf::Vector2i> InputService::getTouchEndedPosition() const {
    return ludork::engine::input_impl::inputImpl().getTouchEndedPosition();
}

std::optional<sf::Vector2i> InputService::getTouchMovedDelta() const {
    return ludork::engine::input_impl::inputImpl().getTouchMovedDelta();
}

void InputService::cancelTouchGesture() noexcept {
    ludork::engine::input_impl::inputImpl().cancelTouchGesture();
}

bool InputService::isTouchTriggered(bool handled) {
    return ludork::engine::input_impl::inputImpl().isTouchTriggered(handled);
}

bool InputService::isTouchBlocked() const {
    return ludork::engine::input_impl::inputImpl().isTouchBlocked();
}

void InputService::blockTouch() {
    ludork::engine::input_impl::inputImpl().blockTouch();
}

void InputService::unblockTouch() {
    ludork::engine::input_impl::inputImpl().unblockTouch();
}

bool InputService::isJoystickButtonPressed() const {
    return ludork::engine::input_impl::inputImpl().isJoystickButtonPressed();
}

bool InputService::isJoystickButtonReleased() const {
    return ludork::engine::input_impl::inputImpl().isJoystickButtonReleased();
}

bool InputService::isMouseInputMode() const {
    return ludork::engine::input_impl::inputImpl().isMouseInputMode();
}

bool InputService::getJoystickButtonPressed(unsigned int joystickId,
                                            unsigned int button, bool handled) {
    return ludork::engine::input_impl::inputImpl().getJoystickButtonPressed(
        joystickId, button, handled);
}

bool InputService::getJoystickButtonValuePressed(unsigned int joystickId,
                                                 const InputNamedValue& button,
                                                 bool handled) {
    return ludork::engine::input_impl::inputImpl()
        .getJoystickButtonValuePressed(joystickId, button, handled);
}

bool InputService::getJoystickButtonReleased(unsigned int joystickId,
                                             unsigned int button,
                                             bool handled) {
    return ludork::engine::input_impl::inputImpl().getJoystickButtonReleased(
        joystickId, button, handled);
}

bool InputService::getJoystickButtonValueReleased(unsigned int joystickId,
                                                  const InputNamedValue& button,
                                                  bool handled) {
    return ludork::engine::input_impl::inputImpl()
        .getJoystickButtonValueReleased(joystickId, button, handled);
}

bool InputService::isJoystickAxisMoved() const {
    return ludork::engine::input_impl::inputImpl().isJoystickAxisMoved();
}

std::optional<JoystickAxisEvent> InputService::getJoystickAxisMoved(
    unsigned int joystickId, bool handled) {
    return ludork::engine::input_impl::inputImpl().getJoystickAxisMoved(
        joystickId, handled);
}

bool InputService::isJoystickConnected() const {
    return ludork::engine::input_impl::inputImpl().isJoystickConnected();
}

bool InputService::isJoystickDisconnected() const {
    return ludork::engine::input_impl::inputImpl().isJoystickDisconnected();
}

bool InputService::isKeyTriggered(sf::Keyboard::Key key, bool alt, bool ctrl,
                                  bool shift, bool system, bool handled,
                                  float repeatDelay, float repeatInterval) {
    return ludork::engine::input_impl::inputImpl().isKeyTriggered(
        key, alt, ctrl, shift, system, handled, repeatDelay, repeatInterval);
}

bool InputService::isJoystickButtonDown(unsigned int joystickId,
                                        unsigned int button) const {
    return ludork::engine::input_impl::inputImpl().isJoystickButtonDown(
        joystickId, button);
}

bool InputService::isJoystickButtonValueDown(
    unsigned int joystickId, const InputNamedValue& button) const {
    return ludork::engine::input_impl::inputImpl().isJoystickButtonValueDown(
        joystickId, button);
}

bool InputService::isAnyJoystickButtonDown(unsigned int button) const {
    return ludork::engine::input_impl::inputImpl().isAnyJoystickButtonDown(
        button);
}

bool InputService::isAnyJoystickButtonValueDown(
    const InputNamedValue& button) const {
    return ludork::engine::input_impl::inputImpl().isAnyJoystickButtonValueDown(
        button);
}

bool InputService::isAnyJoystickButtonTriggered(unsigned int button,
                                                bool handled, float repeatDelay,
                                                float repeatInterval) {
    return ludork::engine::input_impl::inputImpl().isAnyJoystickButtonTriggered(
        button, handled, repeatDelay, repeatInterval);
}

bool InputService::isAnyJoystickButtonValueTriggered(
    const InputNamedValue& button, bool handled, float repeatDelay,
    float repeatInterval) {
    return ludork::engine::input_impl::inputImpl()
        .isAnyJoystickButtonValueTriggered(button, handled, repeatDelay,
                                           repeatInterval);
}

bool InputService::isActionTriggered(
    const std::vector<InputActionKey>& actionKeys, bool handled,
    float repeatDelay, float repeatInterval) {
    return ludork::engine::input_impl::inputImpl().isActionTriggered(
        actionKeys, handled, repeatDelay, repeatInterval);
}

bool InputService::isActionHeld(
    const std::vector<InputActionKey>& actionKeys) const {
    return ludork::engine::input_impl::inputImpl().isActionHeld(actionKeys);
}

bool InputService::isMouseButtonTriggered(sf::Mouse::Button button,
                                          bool handled) {
    return ludork::engine::input_impl::inputImpl().isMouseButtonTriggered(
        button, handled);
}

bool InputService::isMouseButtonDown(sf::Mouse::Button button) const {
    return ludork::engine::input_impl::inputImpl().isMouseButtonDown(button);
}

std::string InputService::getEnteredText() const {
    return ludork::engine::input_impl::inputImpl().getEnteredText();
}

bool InputService::isTextEntered() const {
    return ludork::engine::input_impl::inputImpl().isTextEntered();
}

bool InputService::isKeyboardBlocked() const {
    return ludork::engine::input_impl::inputImpl().isKeyboardBlocked();
}

bool InputService::isMouseBlocked() const {
    return ludork::engine::input_impl::inputImpl().isMouseBlocked();
}

bool InputService::isJoystickBlocked() const {
    return ludork::engine::input_impl::inputImpl().isJoystickBlocked();
}

void InputService::blockKeyboard() {
    ludork::engine::input_impl::inputImpl().blockKeyboard();
}

void InputService::blockMouse() {
    ludork::engine::input_impl::inputImpl().blockMouse();
}

void InputService::blockJoystick() {
    ludork::engine::input_impl::inputImpl().blockJoystick();
}

void InputService::unblockKeyboard() {
    ludork::engine::input_impl::inputImpl().unblockKeyboard();
}

void InputService::unblockMouse() {
    ludork::engine::input_impl::inputImpl().unblockMouse();
}

void InputService::unblockJoystick() {
    ludork::engine::input_impl::inputImpl().unblockJoystick();
}

void InputService::blockInput() {
    ludork::engine::input_impl::inputImpl().blockInput();
}

void InputService::unblockInput() {
    ludork::engine::input_impl::inputImpl().unblockInput();
}

std::vector<InputActionKey> InputService::getConfirmKeys() const {
    return ludork::engine::input_impl::inputImpl().getConfirmKeys();
}

std::vector<InputActionKey> InputService::getCancelKeys() const {
    return ludork::engine::input_impl::inputImpl().getCancelKeys();
}

std::vector<InputActionKey> InputService::getUpKeys() const {
    return ludork::engine::input_impl::inputImpl().getUpKeys();
}

std::vector<InputActionKey> InputService::getDownKeys() const {
    return ludork::engine::input_impl::inputImpl().getDownKeys();
}

std::vector<InputActionKey> InputService::getLeftKeys() const {
    return ludork::engine::input_impl::inputImpl().getLeftKeys();
}

std::vector<InputActionKey> InputService::getRightKeys() const {
    return ludork::engine::input_impl::inputImpl().getRightKeys();
}

void InputService::registerActionMapping(RuntimeIdentityPtr object,
                                         std::string actionName,
                                         std::vector<InputActionKey> actionKeys,
                                         ActionCallback callback,
                                         bool triggerOnHold) {
    ludork::engine::input_impl::inputImpl().registerActionMapping(
        std::move(object), std::move(actionName), std::move(actionKeys),
        std::move(callback), triggerOnHold);
}

void InputService::unregisterActionMapping(const RuntimeIdentityPtr& object,
                                           const std::string& actionName) {
    ludork::engine::input_impl::inputImpl().unregisterActionMapping(object,
                                                                    actionName);
}

void InputService::setFrameCompletionCallback(std::function<void()> callback) {
    ludork::engine::input_impl::inputImpl().setFrameCompletionCallback(
        std::move(callback));
}

void InputService::shutdown() noexcept {
    ludork::engine::input_impl::inputImpl().shutdown();
    FunctionalBase::setInputProvider(nullptr);
}

InputService& inputService() {
    static InputService instance;
    FunctionalBase::setInputProvider(&instance);
    return instance;
}
