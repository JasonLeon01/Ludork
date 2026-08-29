#include <Input/InputService.hpp>

#include "InputService/InputRuntime.hpp"

#include <UI/FunctionalBase.hpp>

#include <utility>

void InputService::initializeNativePolling() {
    inputRuntime().initializeNativePolling();
}

void InputService::update(sf::WindowBase& window) {
    inputRuntime().update(window);
}

void InputService::injectEvent(const InjectedInputEvent& event) {
    inputRuntime().injectEvent(event);
}

void InputService::setUseInjectedMouseOnly(bool value) {
    inputRuntime().setUseInjectedMouseOnly(value);
}

void InputService::setPointerViewport(std::optional<sf::IntRect> viewport) {
    inputRuntime().setPointerViewport(std::move(viewport));
}

void InputService::onWindowRecreated(sf::WindowBase& window) {
    inputRuntime().onWindowRecreated(window);
}

void InputService::requestSystemCancel() noexcept {
    InputEventPump::requestSystemCancel();
}

bool InputService::isFocused() const {
    return inputRuntime().isFocused();
}

bool InputService::isFocusLost() const {
    return inputRuntime().isFocusLost();
}

bool InputService::isFocusGained() const {
    return inputRuntime().isFocusGained();
}

bool InputService::isKeyPressed() const {
    return inputRuntime().isKeyPressed();
}

bool InputService::isKeyReleased() const {
    return inputRuntime().isKeyReleased();
}

bool InputService::getKeyPressed(sf::Keyboard::Key key, bool handled, bool alt,
                                 bool ctrl, bool shift, bool system) {
    return inputRuntime().getKeyPressed(key, handled, alt, ctrl, shift, system);
}

bool InputService::getScanPressed(sf::Keyboard::Scancode scan, bool handled,
                                  bool alt, bool ctrl, bool shift,
                                  bool system) {
    return inputRuntime().getScanPressed(scan, handled, alt, ctrl, shift,
                                         system);
}

bool InputService::getKeyReleased(sf::Keyboard::Key key, bool handled, bool alt,
                                  bool ctrl, bool shift, bool system) {
    return inputRuntime().getKeyReleased(key, handled, alt, ctrl, shift,
                                         system);
}

bool InputService::getScanReleased(sf::Keyboard::Scancode scan, bool handled,
                                   bool alt, bool ctrl, bool shift,
                                   bool system) {
    return inputRuntime().getScanReleased(scan, handled, alt, ctrl, shift,
                                          system);
}

bool InputService::isMouseWheelScrolled() const {
    return inputRuntime().isMouseWheelScrolled();
}

std::optional<sf::Mouse::Wheel> InputService::getMouseScrolledWheel() const {
    return inputRuntime().getMouseScrolledWheel();
}

float InputService::getMouseScrolledWheelDelta() const {
    return inputRuntime().getMouseScrolledWheelDelta();
}

bool InputService::isMouseWheelPrecise() const {
    return inputRuntime().isMouseWheelPrecise();
}

std::optional<sf::Vector2i> InputService::getMouseScrolledWheelPosition()
    const {
    return inputRuntime().getMouseScrolledWheelPosition();
}

bool InputService::isMouseButtonPressed() const {
    return inputRuntime().isMouseButtonPressed();
}

bool InputService::isMouseButtonReleased() const {
    return inputRuntime().isMouseButtonReleased();
}

bool InputService::getMouseButtonPressed(sf::Mouse::Button button,
                                         bool handled) {
    return inputRuntime().getMouseButtonPressed(button, handled);
}

bool InputService::getMouseButtonReleased(sf::Mouse::Button button,
                                          bool handled) {
    return inputRuntime().getMouseButtonReleased(button, handled);
}

bool InputService::isMouseMoved() const {
    return inputRuntime().isMouseMoved();
}

sf::Vector2i InputService::getMousePosition() const {
    return inputRuntime().getMousePosition();
}

std::optional<sf::Vector2i> InputService::getMouseMovedDelta() const {
    return inputRuntime().getMouseMovedDelta();
}

void InputService::setMousePosition(const sf::Vector2i& position) {
    inputRuntime().setMousePosition(position);
}

void InputService::setMousePosition(const sf::Vector2i& position,
                                    sf::WindowBase& window) {
    inputRuntime().setMousePosition(position, window);
}

bool InputService::isMouseEntered() const {
    return inputRuntime().isMouseEntered();
}

bool InputService::isMouseLeft() const {
    return inputRuntime().isMouseLeft();
}

bool InputService::isTouchBegan(bool handled) {
    return inputRuntime().isTouchBegan(handled);
}

bool InputService::isTouchTap(bool handled) {
    return inputRuntime().isTouchTap(handled);
}

bool InputService::isTouchEnded() const {
    return inputRuntime().isTouchEnded();
}

bool InputService::isTouchMoved() const {
    return inputRuntime().isTouchMoved();
}

bool InputService::isTouchDragged() const {
    return inputRuntime().isTouchDragged();
}

bool InputService::isTouchActive() const {
    return inputRuntime().isTouchActive();
}

std::optional<sf::Vector2i> InputService::getTouchPosition() const {
    return inputRuntime().getTouchPosition();
}

std::optional<sf::Vector2i> InputService::getTouchBeganPosition() const {
    return inputRuntime().getTouchBeganPosition();
}

std::optional<sf::Vector2i> InputService::getTouchTapPosition() const {
    return inputRuntime().getTouchTapPosition();
}

std::optional<sf::Vector2i> InputService::getTouchEndedPosition() const {
    return inputRuntime().getTouchEndedPosition();
}

std::optional<sf::Vector2i> InputService::getTouchMovedDelta() const {
    return inputRuntime().getTouchMovedDelta();
}

void InputService::cancelTouchGesture() noexcept {
    inputRuntime().cancelTouchGesture();
}

bool InputService::isTouchTriggered(bool handled) {
    return inputRuntime().isTouchTriggered(handled);
}

bool InputService::isTouchBlocked() const {
    return inputRuntime().isTouchBlocked();
}

void InputService::blockTouch() {
    inputRuntime().blockTouch();
}

void InputService::unblockTouch() {
    inputRuntime().unblockTouch();
}

bool InputService::isJoystickButtonPressed() const {
    return inputRuntime().isJoystickButtonPressed();
}

bool InputService::isJoystickButtonReleased() const {
    return inputRuntime().isJoystickButtonReleased();
}

bool InputService::isMouseInputMode() const {
    return inputRuntime().isMouseInputMode();
}

bool InputService::getJoystickButtonPressed(unsigned int joystickId,
                                            unsigned int button, bool handled) {
    return inputRuntime().getJoystickButtonPressed(joystickId, button, handled);
}

bool InputService::getJoystickButtonValuePressed(unsigned int joystickId,
                                                 const InputNamedValue& button,
                                                 bool handled) {
    return inputRuntime().getJoystickButtonValuePressed(joystickId, button,
                                                        handled);
}

bool InputService::getJoystickButtonReleased(unsigned int joystickId,
                                             unsigned int button,
                                             bool handled) {
    return inputRuntime().getJoystickButtonReleased(joystickId, button,
                                                    handled);
}

bool InputService::getJoystickButtonValueReleased(unsigned int joystickId,
                                                  const InputNamedValue& button,
                                                  bool handled) {
    return inputRuntime().getJoystickButtonValueReleased(joystickId, button,
                                                         handled);
}

bool InputService::isJoystickAxisMoved() const {
    return inputRuntime().isJoystickAxisMoved();
}

std::optional<JoystickAxisEvent> InputService::getJoystickAxisMoved(
    unsigned int joystickId, bool handled) {
    return inputRuntime().getJoystickAxisMoved(joystickId, handled);
}

bool InputService::isJoystickConnected() const {
    return inputRuntime().isJoystickConnected();
}

bool InputService::isJoystickDisconnected() const {
    return inputRuntime().isJoystickDisconnected();
}

bool InputService::isKeyTriggered(sf::Keyboard::Key key, bool alt, bool ctrl,
                                  bool shift, bool system, bool handled,
                                  float repeatDelay, float repeatInterval) {
    return inputRuntime().isKeyTriggered(key, alt, ctrl, shift, system, handled,
                                         repeatDelay, repeatInterval);
}

bool InputService::isAnyJoystickButtonTriggered(unsigned int button,
                                                bool handled, float repeatDelay,
                                                float repeatInterval) {
    return inputRuntime().isAnyJoystickButtonTriggered(
        button, handled, repeatDelay, repeatInterval);
}

bool InputService::isAnyJoystickButtonValueTriggered(
    const InputNamedValue& button, bool handled, float repeatDelay,
    float repeatInterval) {
    return inputRuntime().isAnyJoystickButtonValueTriggered(
        button, handled, repeatDelay, repeatInterval);
}

bool InputService::isActionTriggered(
    const std::vector<InputActionKey>& actionKeys, bool handled,
    float repeatDelay, float repeatInterval) {
    return inputRuntime().isActionTriggered(actionKeys, handled, repeatDelay,
                                            repeatInterval);
}

bool InputService::isActionHeld(
    const std::vector<InputActionKey>& actionKeys) const {
    return inputRuntime().isActionHeld(actionKeys);
}

bool InputService::isMouseButtonTriggered(sf::Mouse::Button button,
                                          bool handled) {
    return inputRuntime().isMouseButtonTriggered(button, handled);
}

bool InputService::isMouseButtonDown(sf::Mouse::Button button) const {
    return inputRuntime().isMouseButtonDown(button);
}

std::string InputService::getEnteredText() const {
    return inputRuntime().getEnteredText();
}

bool InputService::isTextEntered() const {
    return inputRuntime().isTextEntered();
}

bool InputService::isKeyboardBlocked() const {
    return inputRuntime().isKeyboardBlocked();
}

bool InputService::isMouseBlocked() const {
    return inputRuntime().isMouseBlocked();
}

bool InputService::isJoystickBlocked() const {
    return inputRuntime().isJoystickBlocked();
}

void InputService::blockKeyboard() {
    inputRuntime().blockKeyboard();
}

void InputService::blockMouse() {
    inputRuntime().blockMouse();
}

void InputService::blockJoystick() {
    inputRuntime().blockJoystick();
}

void InputService::unblockKeyboard() {
    inputRuntime().unblockKeyboard();
}

void InputService::unblockMouse() {
    inputRuntime().unblockMouse();
}

void InputService::unblockJoystick() {
    inputRuntime().unblockJoystick();
}

void InputService::blockInput() {
    inputRuntime().blockInput();
}

void InputService::unblockInput() {
    inputRuntime().unblockInput();
}

std::vector<InputActionKey> InputService::getConfirmKeys() const {
    return inputRuntime().getConfirmKeys();
}

std::vector<InputActionKey> InputService::getCancelKeys() const {
    return inputRuntime().getCancelKeys();
}

std::vector<InputActionKey> InputService::getUpKeys() const {
    return inputRuntime().getUpKeys();
}

std::vector<InputActionKey> InputService::getDownKeys() const {
    return inputRuntime().getDownKeys();
}

std::vector<InputActionKey> InputService::getLeftKeys() const {
    return inputRuntime().getLeftKeys();
}

std::vector<InputActionKey> InputService::getRightKeys() const {
    return inputRuntime().getRightKeys();
}

void InputService::registerActionMapping(RuntimeIdentityPtr object,
                                         std::string actionName,
                                         std::vector<InputActionKey> actionKeys,
                                         ActionCallback callback,
                                         bool triggerOnHold) {
    inputRuntime().registerActionMapping(
        std::move(object), std::move(actionName), std::move(actionKeys),
        std::move(callback), triggerOnHold);
}

void InputService::unregisterActionMapping(const RuntimeIdentityPtr& object,
                                           const std::string& actionName) {
    inputRuntime().unregisterActionMapping(object, actionName);
}

void InputService::setFrameCompletionCallback(std::function<void()> callback) {
    inputRuntime().setFrameCompletionCallback(std::move(callback));
}

void InputService::shutdown() noexcept {
    inputRuntime().shutdown();
    FunctionalBase::setInputProvider(nullptr);
}

InputService& inputService() {
    static InputService instance;
    FunctionalBase::setInputProvider(&instance);
    return instance;
}
