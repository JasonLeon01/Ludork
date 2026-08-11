#include <Input/InputService.hpp>

#include "PlatformInputBridge.hpp"

#include <Runtime/EngineState.hpp>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/String.hpp>
#include <SFML/Window/Clipboard.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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

InputActionKey keyboardKey(Key key) {
    InputActionKey result;
    result.kind = InputActionKind::Key;
    result.code = static_cast<int>(key);
    return result;
}

InputActionKey keyboardScan(sf::Keyboard::Scancode scan) {
    InputActionKey result;
    result.kind = InputActionKind::Scan;
    result.code = static_cast<int>(scan);
    return result;
}

InputActionKey joystickButton(JoystickButton button) {
    InputActionKey result;
    result.kind = InputActionKind::JoystickButton;
    result.code = static_cast<int>(button);
    for (const auto& [name, value] : inputJoystickButtons) {
        if (value.value == result.code) {
            result.name = name;
            break;
        }
    }
    return result;
}

InputActionKey joystickAxis(sf::Joystick::Axis axis, float threshold,
                            InputAxisComparison comparison) {
    InputActionKey result;
    result.kind = InputActionKind::JoystickAxis;
    result.code = static_cast<int>(axis);
    result.threshold = threshold;
    result.comparison = comparison;
    return result;
}

InputActionKey mouseButton(sf::Mouse::Button button) {
    InputActionKey result;
    result.kind = InputActionKind::MouseButton;
    result.code = static_cast<int>(button);
    return result;
}

InputActionKey touchTap() {
    InputActionKey result;
    result.kind = InputActionKind::TouchTap;
    return result;
}

bool lessThan(float left, float right) {
    return left < right;
}

bool greaterThan(float left, float right) {
    return left > right;
}

constexpr float touchDragThreshold = 8.0f;

}  // namespace

const std::unordered_map<std::string, InputNamedValue> inputJoystickButtons = {
    {"A", {"A", static_cast<int>(JoystickButton::A)}},
    {"B", {"B", static_cast<int>(JoystickButton::B)}},
    {"X", {"X", static_cast<int>(JoystickButton::X)}},
    {"Y", {"Y", static_cast<int>(JoystickButton::Y)}},
    {"LB", {"LB", static_cast<int>(JoystickButton::LB)}},
    {"RB", {"RB", static_cast<int>(JoystickButton::RB)}},
    {"View", {"View", static_cast<int>(JoystickButton::View)}},
    {"Menu", {"Menu", static_cast<int>(JoystickButton::Menu)}},
    {"LS", {"LS", static_cast<int>(JoystickButton::LS)}},
    {"RS", {"RS", static_cast<int>(JoystickButton::RS)}},
    {"XBox", {"XBox", static_cast<int>(JoystickButton::XBox)}},
    {"Share", {"Share", static_cast<int>(JoystickButton::Share)}},
};

const std::unordered_map<std::string, InputNamedValue> inputTypes = {
    {"Mouse", {"Mouse", static_cast<int>(InputType::Mouse)}},
    {"Gamepad", {"Gamepad", static_cast<int>(InputType::Gamepad)}},
};

const std::unordered_map<std::string, int> inputActionKinds = {
    {"KeyOrScan", static_cast<int>(InputActionKind::KeyOrScan)},
    {"Key", static_cast<int>(InputActionKind::Key)},
    {"Scan", static_cast<int>(InputActionKind::Scan)},
    {"MouseButton", static_cast<int>(InputActionKind::MouseButton)},
    {"JoystickButton", static_cast<int>(InputActionKind::JoystickButton)},
    {"JoystickAxis", static_cast<int>(InputActionKind::JoystickAxis)},
    {"TouchTap", static_cast<int>(InputActionKind::TouchTap)},
};

const std::unordered_map<std::string, InputAxisComparison>
    inputAxisComparisons = {
        {"Less", lessThan},
        {"Greater", greaterThan},
};

bool InputActionKey::operator==(const InputActionKey& other) const {
    if (kind != other.kind || name != other.name || code != other.code ||
        threshold != other.threshold) {
        return false;
    }
    if (kind != InputActionKind::JoystickAxis) {
        return true;
    }
    if (comparisonIdentity || other.comparisonIdentity) {
        return comparisonIdentity && other.comparisonIdentity &&
               comparisonIdentity->equals(*other.comparisonIdentity);
    }
    if (!comparison || !other.comparison) {
        return static_cast<bool>(comparison) ==
               static_cast<bool>(other.comparison);
    }
    using FunctionPointer = bool (*)(float, float);
    const FunctionPointer* left = comparison.target<FunctionPointer>();
    const FunctionPointer* right = other.comparison.target<FunctionPointer>();
    return left != nullptr && right != nullptr && *left == *right;
}

std::string InputService::keyId(int code, const Modifiers& modifiers) {
    std::string result = std::to_string(code);
    result.push_back(':');
    result.push_back(modifiers.alt ? '1' : '0');
    result.push_back(':');
    result.push_back(modifiers.control ? '1' : '0');
    result.push_back(':');
    result.push_back(modifiers.shift ? '1' : '0');
    result.push_back(':');
    result.push_back(modifiers.system ? '1' : '0');
    return result;
}

std::string InputService::axisId(unsigned int joystickId,
                                 sf::Joystick::Axis axis) {
    return std::to_string(joystickId) + ":" +
           std::to_string(static_cast<int>(axis));
}

bool InputService::axisMatches(float position, const InputActionKey& key) {
    return key.comparison ? key.comparison(position, key.threshold)
                          : position > key.threshold;
}

sf::Keyboard::Key InputService::keyFromName(const std::string& name) {
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

sf::Mouse::Button InputService::mouseButtonFromName(const std::string& name) {
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

std::string InputService::toUtf8(char32_t codepoint) {
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

sf::Vector2i InputService::pixelToWorld(sf::WindowBase& window,
                                        const sf::Vector2i& pixel) {
    const sf::RenderTarget* target =
        dynamic_cast<const sf::RenderTarget*>(&window);
    if (target == nullptr) {
        return pixel;
    }
    const sf::Vector2f world = target->mapPixelToCoords(pixel);
    return {static_cast<int>(world.x), static_cast<int>(world.y)};
}

sf::Vector2i InputService::worldToPixel(sf::WindowBase& window,
                                        const sf::Vector2i& position) {
    const sf::RenderTarget* target =
        dynamic_cast<const sf::RenderTarget*>(&window);
    if (target == nullptr) {
        return position;
    }
    return target->mapCoordsToPixel(
        {static_cast<float>(position.x), static_cast<float>(position.y)});
}

void InputService::resetFrameState() {
    for (const std::string& identifier : pendingKeyTriggerReleases_) {
        keyTriggers_.erase(identifier);
    }
    for (const std::string& identifier : pendingScanTriggerReleases_) {
        scanTriggers_.erase(identifier);
    }
    for (const int button : pendingMouseTriggerReleases_) {
        mouseTriggers_.erase(button);
    }
    pendingKeyTriggerReleases_.clear();
    pendingScanTriggerReleases_.clear();
    pendingMouseTriggerReleases_.clear();
    focusLost_ = false;
    focusGained_ = false;
    keyPressed_ = false;
    keyReleased_ = false;
    keyPressedEvents_.clear();
    keyReleasedEvents_.clear();
    scanPressedEvents_.clear();
    scanReleasedEvents_.clear();
    mouseWheelScrolled_ = false;
    mouseWheel_.reset();
    mouseWheelDelta_ = 0.0f;
    mouseWheelPrecise_ = false;
    mouseWheelPosition_.reset();
    mouseButtonPressed_ = false;
    mouseButtonReleased_ = false;
    mousePressedEvents_.clear();
    mouseReleasedEvents_.clear();
    mouseMoved_ = false;
    mouseMovedDelta_.reset();
    mouseEntered_ = false;
    mouseLeft_ = false;
    touchBegan_ = false;
    touchEnded_ = false;
    touchMoved_ = false;
    touchBeganPosition_.reset();
    touchTap_ = false;
    touchTapHandled_ = false;
    touchTapPosition_.reset();
    touchEndedPosition_.reset();
    touchMovedDelta_.reset();
    touchBeganHandled_ = false;
    touchCancelMousePressedThisFrame_ = false;
    joystickButtonPressed_ = false;
    joystickButtonReleased_ = false;
    joystickAxisMoved_ = false;
    joystickConnected_ = false;
    joystickDisconnected_ = false;
    joystickPressedEvents_.clear();
    joystickReleasedEvents_.clear();
    joystickAxisEvents_.clear();
    enteredText_.clear();
}

void InputService::clearKeyboardState() {
    keyPressed_ = false;
    keyReleased_ = false;
    keyPressedEvents_.clear();
    keyReleasedEvents_.clear();
    scanPressedEvents_.clear();
    scanReleasedEvents_.clear();
    keyTriggers_.clear();
    scanTriggers_.clear();
    pendingKeyTriggerReleases_.clear();
    pendingScanTriggerReleases_.clear();
    heldKeys_.clear();
    heldScans_.clear();
}

void InputService::setFocused(bool focused) {
    if (focused_ == focused) {
        return;
    }
    focused_ = focused;
    if (focused) {
        focusGained_ = true;
        return;
    }
    focusLost_ = true;
    abortTwoFingerCancel();
    touchTravelDistance_ = 0.0f;
    touchDragged_ = false;
    touchGestureSuppressed_ = true;
    touchFingers_.clear();
    touchActive_ = false;
    touchTrigger_ = {};
    clearKeyboardState();
}

void InputService::setKeyPressed(sf::Keyboard::Key key,
                                 sf::Keyboard::Scancode scan,
                                 const Modifiers& modifiers) {
    keyPressed_ = true;
    const std::string keyIdentifier = keyId(static_cast<int>(key), modifiers);
    const std::string scanIdentifier = keyId(static_cast<int>(scan), modifiers);
    pendingKeyTriggerReleases_.erase(keyIdentifier);
    pendingScanTriggerReleases_.erase(scanIdentifier);
    keyPressedEvents_[keyIdentifier] = true;
    scanPressedEvents_[scanIdentifier] = true;
    heldKeys_.insert(static_cast<int>(key));
    heldScans_.insert(static_cast<int>(scan));
    TriggerEntry& keyEntry = keyTriggers_[keyIdentifier];
    ++keyEntry.count;
    TriggerEntry& scanEntry = scanTriggers_[scanIdentifier];
    ++scanEntry.count;
}

void InputService::setKeyReleased(sf::Keyboard::Key key,
                                  sf::Keyboard::Scancode scan,
                                  const Modifiers& modifiers) {
    keyReleased_ = true;
    const std::string keyIdentifier = keyId(static_cast<int>(key), modifiers);
    const std::string scanIdentifier = keyId(static_cast<int>(scan), modifiers);
    keyReleasedEvents_[keyIdentifier] = true;
    scanReleasedEvents_[scanIdentifier] = true;
    if (keyPressedEvents_.contains(keyIdentifier)) {
        pendingKeyTriggerReleases_.insert(keyIdentifier);
    } else {
        keyTriggers_.erase(keyIdentifier);
        pendingKeyTriggerReleases_.erase(keyIdentifier);
    }
    if (scanPressedEvents_.contains(scanIdentifier)) {
        pendingScanTriggerReleases_.insert(scanIdentifier);
    } else {
        scanTriggers_.erase(scanIdentifier);
        pendingScanTriggerReleases_.erase(scanIdentifier);
    }
    heldKeys_.erase(static_cast<int>(key));
    heldScans_.erase(static_cast<int>(scan));
}

void InputService::setMouseButtonPressed(sf::Mouse::Button button,
                                         const sf::Vector2i& position) {
    const int buttonCode = static_cast<int>(button);
    mouseButtonPressed_ = true;
    pendingMouseTriggerReleases_.erase(buttonCode);
    mousePressedEvents_[buttonCode] = true;
    mousePosition_ = position;
    TriggerEntry& entry = mouseTriggers_[buttonCode];
    ++entry.count;
}

void InputService::setMouseButtonReleased(sf::Mouse::Button button,
                                          const sf::Vector2i& position) {
    const int buttonCode = static_cast<int>(button);
    mouseButtonReleased_ = true;
    mouseReleasedEvents_[buttonCode] = true;
    mousePosition_ = position;
    if (mousePressedEvents_.contains(buttonCode)) {
        pendingMouseTriggerReleases_.insert(buttonCode);
    } else {
        mouseTriggers_.erase(buttonCode);
        pendingMouseTriggerReleases_.erase(buttonCode);
    }
}

void InputService::beginTwoFingerCancel(const sf::Vector2i& position) {
    if (touchCancelMouseActive_) {
        return;
    }
    touchGestureSuppressed_ = true;
    touchMovedDelta_.reset();
    setMouseButtonPressed(sf::Mouse::Button::Right, position);
    touchCancelMouseActive_ = true;
    touchCancelMousePressedThisFrame_ = true;
    touchBeganHandled_ = true;
    touchTrigger_ = {0, true};
}

void InputService::endTwoFingerCancel(const sf::Vector2i& position) {
    if (!touchCancelMouseActive_) {
        return;
    }
    touchCancelMouseActive_ = false;
    if (touchCancelMousePressedThisFrame_) {
        touchCancelMouseReleasePending_ = position;
        return;
    }
    setMouseButtonReleased(sf::Mouse::Button::Right, position);
}

void InputService::releasePendingTwoFingerCancel() {
    if (!touchCancelMouseReleasePending_.has_value()) {
        return;
    }
    const sf::Vector2i position = *touchCancelMouseReleasePending_;
    touchCancelMouseReleasePending_.reset();
    setMouseButtonReleased(sf::Mouse::Button::Right, position);
}

void InputService::abortTwoFingerCancel() {
    const bool syntheticMouseActive =
        touchCancelMouseActive_ || touchCancelMouseReleasePending_.has_value();
    const int button = static_cast<int>(sf::Mouse::Button::Right);
    touchCancelMouseActive_ = false;
    touchCancelMousePressedThisFrame_ = false;
    touchCancelMouseReleasePending_.reset();
    if (!syntheticMouseActive) {
        return;
    }
    mouseTriggers_.erase(button);
    pendingMouseTriggerReleases_.erase(button);
    mousePressedEvents_.erase(button);
    mouseReleasedEvents_.erase(button);
}

void InputService::injectEvent(const InjectedInputEvent& event) {
    injectedEvents_.push_back(event);
}

void InputService::setUseInjectedMouseOnly(bool value) {
    useInjectedMouseOnly_ = value;
    if (!value) {
        injectedPointerPixel_.reset();
        injectedPointerTransitionPending_.reset();
    }
}

void InputService::setPointerViewport(std::optional<sf::IntRect> viewport) {
    pointerViewport_ = std::move(viewport);
    if (!useInjectedMouseOnly_ || !injectedPointerPixel_.has_value() ||
        activeWindow_ == nullptr) {
        return;
    }
    const bool inside = acceptsPointerPixel(*injectedPointerPixel_);
    if (!injectedPointerTransitionPending_.has_value()) {
        if (inside != pointerInsideViewport_) {
            injectedPointerTransitionPending_ = inside;
        }
    } else if (inside == !*injectedPointerTransitionPending_) {
        injectedPointerTransitionPending_.reset();
    }
    pointerInsideViewport_ = inside;
    mousePosition_ = pixelToWorld(*activeWindow_, *injectedPointerPixel_);
}

void InputService::onWindowRecreated(sf::WindowBase& window) {
    resetFrameState();
    clearKeyboardState();
    mouseButtonPressed_ = false;
    mouseButtonReleased_ = false;
    mousePressedEvents_.clear();
    mouseReleasedEvents_.clear();
    mouseTriggers_.clear();
    pendingMouseTriggerReleases_.clear();
    mouseMoved_ = false;
    mouseMovedDelta_.reset();
    mouseEntered_ = false;
    mouseLeft_ = false;
    touchBegan_ = false;
    touchEnded_ = false;
    touchMoved_ = false;
    touchActive_ = false;
    touchPosition_.reset();
    touchBeganPosition_.reset();
    touchTap_ = false;
    touchTapHandled_ = false;
    touchTapPosition_.reset();
    touchEndedPosition_.reset();
    touchMovedDelta_.reset();
    touchBeganHandled_ = false;
    touchTrigger_ = {};
    touchTravelDistance_ = 0.0f;
    touchDragged_ = false;
    touchGestureSuppressed_ = false;
    touchFingers_.clear();
    touchCancelMouseActive_ = false;
    touchCancelMousePressedThisFrame_ = false;
    touchCancelMouseReleasePending_.reset();
    injectedPointerPixel_.reset();
    injectedPointerTransitionPending_.reset();
    focused_ = window.hasFocus();
    activeWindow_ = &window;
    pointerInsideViewport_ = acceptsPointerPixel(sf::Mouse::getPosition(window));
    window.setMouseCursorVisible(currentInputType_ == InputType::Mouse);
}

bool InputService::acceptsPointerPixel(const sf::Vector2i& pixel) const {
    return !pointerViewport_.has_value() || pointerViewport_->contains(pixel);
}

void InputService::updatePointerViewportState(bool inside) {
    if (inside == pointerInsideViewport_) {
        return;
    }
    pointerInsideViewport_ = inside;
    if (inside) {
        mouseEntered_ = true;
    } else {
        mouseLeft_ = true;
    }
}

void InputService::updatePointerViewportState(const sf::Vector2i& pixel) {
    updatePointerViewportState(acceptsPointerPixel(pixel));
}

void InputService::recordMouseWheel(sf::Mouse::Wheel wheel, float delta,
                                    const sf::Vector2i& position,
                                    bool precise) {
    if (delta == 0.0f) {
        return;
    }
    if (mouseWheelScrolled_ && mouseWheel_ == sf::Mouse::Wheel::Vertical &&
        wheel != sf::Mouse::Wheel::Vertical) {
        return;
    }
    if (!mouseWheelScrolled_ || mouseWheel_ != wheel ||
        mouseWheelPrecise_ != precise) {
        mouseWheelDelta_ = 0.0f;
    }
    mouseWheelScrolled_ = true;
    mouseWheel_ = wheel;
    mouseWheelDelta_ += delta;
    mouseWheelPrecise_ = precise;
    mouseWheelPosition_ = position;
}

void InputService::processPlatformScrollEvents(sf::WindowBase& window) {
    if (!ludork::engine::platform_input::isScrollCaptureAvailable()) {
        return;
    }
    const std::vector<ludork::engine::platform_input::ScrollEvent> events =
        ludork::engine::platform_input::consumeScrollEvents(window);
    if (events.empty()) {
        return;
    }
    mouseWheelScrolled_ = false;
    mouseWheel_.reset();
    mouseWheelDelta_ = 0.0f;
    mouseWheelPrecise_ = false;
    mouseWheelPosition_.reset();
    if (useInjectedMouseOnly_ || !window.hasFocus()) {
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

void InputService::processInjectedEvents() {
    while (!injectedEvents_.empty()) {
        const InjectedInputEvent event = std::move(injectedEvents_.front());
        injectedEvents_.pop_front();
        const Modifiers modifiers{event.alt, event.control, event.shift,
                                  event.system};
        const sf::Vector2i pixel{event.x, event.y};
        const sf::Vector2i position =
            activeWindow_ == nullptr ? pixel
                                     : pixelToWorld(*activeWindow_, pixel);
        if (event.type == "MouseMoved" ||
            event.type == "MouseButtonPressed" ||
            event.type == "MouseButtonReleased" ||
            event.type == "MouseWheelScrolled" ||
            event.type == "MouseEntered") {
            injectedPointerPixel_ = pixel;
        }
        if (event.type == "KeyPressed") {
            setFocused(true);
            setKeyPressed(keyFromName(event.key),
                          sf::Keyboard::Scancode::Unknown, modifiers);
        } else if (event.type == "KeyReleased") {
            setFocused(true);
            setKeyReleased(keyFromName(event.key),
                           sf::Keyboard::Scancode::Unknown, modifiers);
        } else if (event.type == "MouseMoved") {
            updatePointerViewportState(pixel);
            if (acceptsPointerPixel(pixel) || !mouseTriggers_.empty()) {
                const sf::Vector2i previous = mousePosition_;
                mouseMoved_ = true;
                mousePosition_ = position;
                if (mousePosition_ != previous) {
                    mouseMovedDelta_ = mousePosition_ - previous;
                }
            } else {
                mousePosition_ = position;
            }
        } else if (event.type == "MouseButtonPressed") {
            updatePointerViewportState(pixel);
            if (acceptsPointerPixel(pixel)) {
                setMouseButtonPressed(mouseButtonFromName(event.button),
                                      position);
            }
        } else if (event.type == "MouseButtonReleased") {
            updatePointerViewportState(pixel);
            const sf::Mouse::Button button =
                mouseButtonFromName(event.button);
            if (mouseTriggers_.contains(static_cast<int>(button))) {
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
            injectedPointerPixel_.reset();
            injectedPointerTransitionPending_.reset();
            updatePointerViewportState(false);
            if (mouseTriggers_.empty()) {
                const int outside = std::numeric_limits<int>::lowest() / 2;
                mousePosition_ = {outside, outside};
                mouseMoved_ = false;
                mouseMovedDelta_.reset();
            }
        }
    }
}

bool InputService::processNativeEvent(sf::WindowBase& window,
                                      const sf::Event& event) {
    if (event.is<sf::Event::Closed>()) {
        window.close();
    }
    if (!useInjectedMouseOnly_ && event.is<sf::Event::FocusLost>()) {
        setFocused(false);
    }
    if (!useInjectedMouseOnly_ && event.is<sf::Event::FocusGained>()) {
        setFocused(true);
    }
    if (!useInjectedMouseOnly_ && !window.hasFocus()) {
        return false;
    }

    if (!useInjectedMouseOnly_) {
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
            if (mouseTriggers_.contains(static_cast<int>(mouseEvent->button))) {
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
                !mouseTriggers_.empty()) {
                const sf::Vector2i previous = mousePosition_;
                mouseMoved_ = true;
                mousePosition_ = position;
                if (mousePosition_ != previous) {
                    mouseMovedDelta_ = mousePosition_ - previous;
                }
            } else {
                mousePosition_ = position;
            }
        }
        if (event.is<sf::Event::MouseEntered>()) {
            updatePointerViewportState(sf::Mouse::getPosition(window));
        }
        if (event.is<sf::Event::MouseLeft>()) {
            updatePointerViewportState(false);
        }
    }

    if (!touchBlocked_) {
        if (const sf::Event::TouchBegan* touchEvent =
                event.getIf<sf::Event::TouchBegan>()) {
            if (!acceptsPointerPixel(touchEvent->position)) {
                return true;
            }
            const sf::Vector2i position =
                pixelToWorld(window, touchEvent->position);
            touchFingers_[touchEvent->finger] = position;
            if (touchEvent->finger == 0) {
                touchBegan_ = true;
                touchActive_ = true;
                touchBeganPosition_ = position;
                touchPosition_ = position;
                touchTravelDistance_ = 0.0f;
                touchDragged_ = false;
                touchGestureSuppressed_ = false;
                ++touchTrigger_.count;
                touchTrigger_.handled = false;
            }
            if (touchFingers_.size() >= 2) {
                beginTwoFingerCancel(position);
            }
        }
        if (const sf::Event::TouchMoved* touchEvent =
                event.getIf<sf::Event::TouchMoved>()) {
            const sf::Vector2i position =
                pixelToWorld(window, touchEvent->position);
            const auto finger = touchFingers_.find(touchEvent->finger);
            if (finger != touchFingers_.end()) {
                finger->second = position;
            }
            if (touchEvent->finger == 0 && finger != touchFingers_.end()) {
                const std::optional<sf::Vector2i> previous = touchPosition_;
                touchMoved_ = true;
                touchPosition_ = position;
                if (previous.has_value() && position != *previous) {
                    const sf::Vector2i delta = position - *previous;
                    touchMovedDelta_ = delta;
                    touchTravelDistance_ +=
                        std::hypot(static_cast<float>(delta.x),
                                   static_cast<float>(delta.y));
                    if (touchTravelDistance_ > touchDragThreshold * Scale) {
                        touchDragged_ = true;
                        touchTrigger_.handled = true;
                    }
                }
            }
        }
        if (const sf::Event::TouchEnded* touchEvent =
                event.getIf<sf::Event::TouchEnded>()) {
            const sf::Vector2i position =
                pixelToWorld(window, touchEvent->position);
            const auto finger = touchFingers_.find(touchEvent->finger);
            const bool primaryGesture = touchEvent->finger == 0 &&
                                        finger != touchFingers_.end() &&
                                        touchActive_;
            if (finger != touchFingers_.end()) {
                touchFingers_.erase(finger);
            }
            if (primaryGesture) {
                const std::optional<sf::Vector2i> previous = touchPosition_;
                if (previous.has_value() && position != *previous) {
                    const sf::Vector2i delta = position - *previous;
                    touchMoved_ = true;
                    touchMovedDelta_ = delta;
                    touchTravelDistance_ +=
                        std::hypot(static_cast<float>(delta.x),
                                   static_cast<float>(delta.y));
                    if (touchTravelDistance_ > touchDragThreshold * Scale) {
                        touchDragged_ = true;
                        touchTrigger_.handled = true;
                    }
                }
                touchEnded_ = true;
                touchActive_ = false;
                touchEndedPosition_ = position;
                touchPosition_ = position;
                if (!touchDragged_ && !touchGestureSuppressed_) {
                    touchTap_ = true;
                    touchTapPosition_ = position;
                }
                touchTrigger_ = {};
            }
            if (touchCancelMouseActive_ && touchFingers_.size() < 2) {
                endTwoFingerCancel(position);
            }
        }
    }

    if (const sf::Event::JoystickButtonPressed* joystickEvent =
            event.getIf<sf::Event::JoystickButtonPressed>()) {
        joystickButtonPressed_ = true;
        joystickPressedEvents_[joystickEvent->joystickId]
                              [joystickEvent->button] = true;
        TriggerEntry& entry = joystickButtonTriggers_[joystickEvent->button];
        ++entry.count;
    }
    if (const sf::Event::JoystickButtonReleased* joystickEvent =
            event.getIf<sf::Event::JoystickButtonReleased>()) {
        joystickButtonReleased_ = true;
        joystickReleasedEvents_[joystickEvent->joystickId]
                               [joystickEvent->button] = true;
        joystickButtonTriggers_.erase(joystickEvent->button);
    }
    if (const sf::Event::JoystickMoved* joystickEvent =
            event.getIf<sf::Event::JoystickMoved>()) {
        joystickAxisMoved_ = true;
        joystickAxisEvents_[joystickEvent->joystickId][joystickEvent->axis] =
            joystickEvent->position;
        joystickAxisStatus_[joystickEvent->joystickId][joystickEvent->axis] =
            joystickEvent->position;
    }
    joystickConnected_ =
        joystickConnected_ || event.is<sf::Event::JoystickConnected>();
    if (const sf::Event::JoystickDisconnected* joystickEvent =
            event.getIf<sf::Event::JoystickDisconnected>()) {
        joystickDisconnected_ = true;
        joystickAxisStatus_.erase(joystickEvent->joystickId);
        joystickPressedEvents_.erase(joystickEvent->joystickId);
        joystickReleasedEvents_.erase(joystickEvent->joystickId);
    }
    if (!useInjectedMouseOnly_) {
        if (const sf::Event::TextEntered* textEvent =
                event.getIf<sf::Event::TextEntered>()) {
            enteredText_ += toUtf8(textEvent->unicode);
        }
    }
    return true;
}

void InputService::updateJoystickDominantAxes() {
    for (const auto& [joystickId, axes] : joystickAxisStatus_) {
        std::optional<sf::Joystick::Axis> dominant;
        float maximum = 0.0f;
        for (const auto& [axis, position] : axes) {
            const float magnitude = std::abs(position);
            if (magnitude > maximum) {
                maximum = magnitude;
                dominant = axis;
            }
        }
        if (maximum < 10.0f) {
            dominant.reset();
        }
        const auto previousIterator = joystickDominantAxis_.find(joystickId);
        const std::optional<sf::Joystick::Axis> previous =
            previousIterator == joystickDominantAxis_.end()
                ? std::optional<sf::Joystick::Axis>{}
                : previousIterator->second;
        if (dominant != previous) {
            if (previous.has_value()) {
                joystickAxisTriggers_.erase(axisId(joystickId, *previous));
            }
            if (dominant.has_value()) {
                joystickAxisTriggers_[axisId(joystickId, *dominant)] = {1,
                                                                        false};
            }
            joystickDominantAxis_[joystickId] = dominant;
        }
    }
}

void InputService::updateInputType(sf::WindowBase& window) {
    std::optional<InputType> next;
    if (mouseMoved_ || mouseButtonPressed_ || mouseWheelScrolled_) {
        next = InputType::Mouse;
    } else if (keyPressed_ || joystickButtonPressed_ ||
               !joystickAxisTriggers_.empty()) {
        next = InputType::Gamepad;
    } else if (joystickAxisMoved_) {
        for (const auto& [joystickId, axes] : joystickAxisStatus_) {
            static_cast<void>(joystickId);
            if (std::any_of(axes.begin(), axes.end(), [](const auto& entry) {
                    return std::abs(entry.second) > 10.0f;
                })) {
                next = InputType::Gamepad;
                break;
            }
        }
    }
    if (next.has_value() && *next != currentInputType_) {
        currentInputType_ = *next;
        window.setMouseCursorVisible(currentInputType_ == InputType::Mouse);
    }
}

void InputService::initializeNativePolling() {
    if (useInjectedMouseOnly_) {
        return;
    }
    static_cast<void>(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A));
    ludork::engine::platform_input::initialize();
}

bool InputService::isKeyboardKeyDown(sf::Keyboard::Key key) const {
    if (heldKeys_.contains(static_cast<int>(key))) {
        return true;
    }
    return !useInjectedMouseOnly_ && sf::Keyboard::isKeyPressed(key);
}

bool InputService::isKeyboardScanDown(sf::Keyboard::Scancode scan) const {
    if (heldScans_.contains(static_cast<int>(scan))) {
        return true;
    }
    return !useInjectedMouseOnly_ && sf::Keyboard::isKeyPressed(scan);
}

bool InputService::isAnyJoystickButtonDown(unsigned int button) const {
    for (unsigned int joystickId = 0; joystickId < sf::Joystick::Count;
         ++joystickId) {
        if (sf::Joystick::isConnected(joystickId) &&
            sf::Joystick::isButtonPressed(joystickId, button)) {
            return true;
        }
    }
    return false;
}

void InputService::dispatchActionMappings() {
    struct MoveAction {
        float position = 0.0f;
        ActionCallback callback;
        RuntimeIdentityPtr object;
    };
    std::vector<MoveAction> moveActions;
    const std::vector<ActionMapping> mappings = actionMappings_;
    for (const ActionMapping& mapping : mappings) {
        if (!mapping.callback) {
            continue;
        }
        for (const InputActionKey& key : mapping.actionKeys) {
            if (key.kind == InputActionKind::JoystickButton) {
                if (joystickBlocked_) {
                    continue;
                }
                bool triggered = false;
                for (const auto& [joystickId, buttons] :
                     joystickPressedEvents_) {
                    static_cast<void>(joystickId);
                    const auto iterator =
                        buttons.find(static_cast<unsigned int>(key.code));
                    if (iterator != buttons.end() && iterator->second) {
                        triggered = true;
                        break;
                    }
                }
                if (!triggered && mapping.triggerOnHold) {
                    triggered = isAnyJoystickButtonDown(
                        static_cast<unsigned int>(key.code));
                }
                if (triggered) {
                    mapping.callback(mapping.object, std::nullopt);
                }
            } else if (key.kind == InputActionKind::JoystickAxis) {
                if (joystickBlocked_) {
                    continue;
                }
                const sf::Joystick::Axis axis =
                    static_cast<sf::Joystick::Axis>(key.code);
                for (const auto& [joystickId, axes] : joystickAxisStatus_) {
                    static_cast<void>(joystickId);
                    const auto iterator = axes.find(axis);
                    if (iterator != axes.end() &&
                        std::abs(iterator->second) > 1e-6f &&
                        axisMatches(iterator->second, key)) {
                        moveActions.push_back({iterator->second,
                                               mapping.callback,
                                               mapping.object});
                    }
                }
            } else if (key.kind == InputActionKind::MouseButton) {
                if (!mouseBlocked_ &&
                    (getMouseButtonPressed(
                         static_cast<sf::Mouse::Button>(key.code), false) ||
                     (mapping.triggerOnHold &&
                      isMouseButtonDown(
                          static_cast<sf::Mouse::Button>(key.code))))) {
                    mapping.callback(mapping.object, std::nullopt);
                }
            } else if (key.kind == InputActionKind::TouchTap) {
                if (isTouchTap(false)) {
                    mapping.callback(mapping.object, std::nullopt);
                }
            } else if (focused_ && !keyboardBlocked_) {
                bool triggered = false;
                const Modifiers modifiers{};
                if (key.kind == InputActionKind::Scan) {
                    const std::string identifier = keyId(key.code, modifiers);
                    const auto iterator = scanPressedEvents_.find(identifier);
                    triggered = iterator != scanPressedEvents_.end() &&
                                iterator->second;
                    if (!triggered && mapping.triggerOnHold) {
                        triggered = isKeyboardScanDown(
                            static_cast<sf::Keyboard::Scancode>(key.code));
                    }
                } else if (key.kind == InputActionKind::Key) {
                    const std::string identifier = keyId(key.code, modifiers);
                    const auto iterator = keyPressedEvents_.find(identifier);
                    triggered =
                        iterator != keyPressedEvents_.end() && iterator->second;
                    if (!triggered && mapping.triggerOnHold) {
                        triggered = isKeyboardKeyDown(
                            static_cast<sf::Keyboard::Key>(key.code));
                    }
                } else if (key.kind == InputActionKind::KeyOrScan) {
                    const std::string identifier = keyId(key.code, modifiers);
                    const auto keyIterator = keyPressedEvents_.find(identifier);
                    const auto scanIterator =
                        scanPressedEvents_.find(identifier);
                    const bool keyTriggered =
                        keyIterator != keyPressedEvents_.end() &&
                        keyIterator->second;
                    const bool scanTriggered =
                        scanIterator != scanPressedEvents_.end() &&
                        scanIterator->second;
                    triggered = keyTriggered || scanTriggered;
                    if (!triggered && mapping.triggerOnHold) {
                        triggered =
                            isKeyboardKeyDown(
                                static_cast<sf::Keyboard::Key>(key.code)) ||
                            isKeyboardScanDown(
                                static_cast<sf::Keyboard::Scancode>(key.code));
                    }
                }
                if (triggered) {
                    mapping.callback(mapping.object, std::nullopt);
                }
            }
        }
    }

    const auto finalAction = std::max_element(
        moveActions.begin(), moveActions.end(),
        [](const MoveAction& left, const MoveAction& right) {
            return std::abs(left.position) < std::abs(right.position);
        });
    if (finalAction != moveActions.end() && finalAction->position != 0.0f) {
        finalAction->callback(finalAction->object, finalAction->position);
    }
}

void InputService::update(sf::WindowBase& window) {
    activeWindow_ = &window;
    resetFrameState();
    if (injectedPointerTransitionPending_.has_value()) {
        if (*injectedPointerTransitionPending_) {
            mouseEntered_ = true;
        } else {
            mouseLeft_ = true;
        }
        injectedPointerTransitionPending_.reset();
    }
    releasePendingTwoFingerCancel();
    processInjectedEvents();
    if (!useInjectedMouseOnly_) {
        setFocused(window.hasFocus());
    }

    while (const std::optional<sf::Event> event = window.pollEvent()) {
        if (!processNativeEvent(window, *event)) {
            break;
        }
    }
    processPlatformScrollEvents(window);

    if (!useInjectedMouseOnly_ && window.hasFocus()) {
        const sf::Vector2i pixel = sf::Mouse::getPosition(window);
        updatePointerViewportState(pixel);
        const sf::Vector2i polled = pixelToWorld(window, pixel);
        if (acceptsPointerPixel(pixel) || !mouseTriggers_.empty()) {
            if (polled != mousePosition_) {
                const sf::Vector2i previous = mousePosition_;
                mousePosition_ = polled;
                mouseMoved_ = true;
                mouseMovedDelta_ = polled - previous;
            }
        } else {
            mousePosition_ = polled;
        }
    }

    if (enteredText_.size() == 1 && enteredText_[0] == '\x16') {
        const sf::U8String clipboard = sf::Clipboard::getString().toUtf8();
        enteredText_.assign(reinterpret_cast<const char*>(clipboard.data()),
                            clipboard.size());
    }

    updateJoystickDominantAxes();
    updateInputType(window);
    dispatchActionMappings();
    if (frameCompletionCallback_) {
        frameCompletionCallback_();
    }
}

bool InputService::isFocused() const {
    return focused_;
}

bool InputService::isFocusLost() const {
    return focusLost_;
}

bool InputService::isFocusGained() const {
    return focusGained_;
}

bool InputService::isKeyPressed() const {
    return keyPressed_ && focused_ && !keyboardBlocked_;
}

bool InputService::isKeyReleased() const {
    return keyReleased_ && focused_ && !keyboardBlocked_;
}

bool InputService::consume(std::unordered_map<std::string, bool>& events,
                           const std::string& id, bool handled) {
    const auto iterator = events.find(id);
    if (iterator == events.end()) {
        return false;
    }
    const bool result = iterator->second;
    if (result && handled) {
        iterator->second = false;
    }
    return result;
}

bool InputService::getKeyPressed(sf::Keyboard::Key key, bool handled, bool alt,
                                 bool ctrl, bool shift, bool system) {
    if (!isKeyPressed()) {
        return false;
    }
    const Modifiers modifiers{alt, ctrl, shift, system};
    const std::string identifier = keyId(static_cast<int>(key), modifiers);
    const auto direct = keyPressedEvents_.find(identifier);
    if (direct != keyPressedEvents_.end()) {
        const bool result = direct->second;
        if (result && handled) {
            direct->second = false;
        }
        return result;
    }
    const sf::Keyboard::Scancode scan = sf::Keyboard::delocalize(key);
    if (scan == sf::Keyboard::Scancode::Unknown) {
        return false;
    }
    const bool result = consume(
        scanPressedEvents_, keyId(static_cast<int>(scan), modifiers), handled);
    if (result && handled) {
        const auto unknown = keyPressedEvents_.find(
            keyId(static_cast<int>(Key::Unknown), modifiers));
        if (unknown != keyPressedEvents_.end()) {
            unknown->second = false;
        }
    }
    return result;
}

bool InputService::getScanPressed(sf::Keyboard::Scancode scan, bool handled,
                                  bool alt, bool ctrl, bool shift,
                                  bool system) {
    if (!isKeyPressed()) {
        return false;
    }
    return consume(scanPressedEvents_,
                   keyId(static_cast<int>(scan), {alt, ctrl, shift, system}),
                   handled);
}

bool InputService::getKeyReleased(sf::Keyboard::Key key, bool handled, bool alt,
                                  bool ctrl, bool shift, bool system) {
    if (!isKeyReleased()) {
        return false;
    }
    const Modifiers modifiers{alt, ctrl, shift, system};
    const std::string identifier = keyId(static_cast<int>(key), modifiers);
    const auto direct = keyReleasedEvents_.find(identifier);
    if (direct != keyReleasedEvents_.end()) {
        const bool result = direct->second;
        if (result && handled) {
            direct->second = false;
        }
        return result;
    }
    const sf::Keyboard::Scancode scan = sf::Keyboard::delocalize(key);
    if (scan == sf::Keyboard::Scancode::Unknown) {
        return false;
    }
    const bool result = consume(
        scanReleasedEvents_, keyId(static_cast<int>(scan), modifiers), handled);
    if (result && handled) {
        const auto unknown = keyReleasedEvents_.find(
            keyId(static_cast<int>(Key::Unknown), modifiers));
        if (unknown != keyReleasedEvents_.end()) {
            unknown->second = false;
        }
    }
    return result;
}

bool InputService::getScanReleased(sf::Keyboard::Scancode scan, bool handled,
                                   bool alt, bool ctrl, bool shift,
                                   bool system) {
    if (!isKeyReleased()) {
        return false;
    }
    return consume(scanReleasedEvents_,
                   keyId(static_cast<int>(scan), {alt, ctrl, shift, system}),
                   handled);
}

bool InputService::isMouseWheelScrolled() const {
    return mouseWheelScrolled_ && !mouseBlocked_;
}

std::optional<sf::Mouse::Wheel> InputService::getMouseScrolledWheel() const {
    return isMouseWheelScrolled() ? mouseWheel_
                                  : std::optional<sf::Mouse::Wheel>{};
}

float InputService::getMouseScrolledWheelDelta() const {
    return isMouseWheelScrolled() ? mouseWheelDelta_ : 0.0f;
}

bool InputService::isMouseWheelPrecise() const {
    return isMouseWheelScrolled() && mouseWheelPrecise_;
}

std::optional<sf::Vector2i> InputService::getMouseScrolledWheelPosition()
    const {
    return isMouseWheelScrolled() ? mouseWheelPosition_
                                  : std::optional<sf::Vector2i>{};
}

bool InputService::isMouseButtonPressed() const {
    return mouseButtonPressed_ && !mouseBlocked_;
}

bool InputService::isMouseButtonReleased() const {
    return mouseButtonReleased_ && !mouseBlocked_;
}

bool InputService::getMouseButtonPressed(sf::Mouse::Button button,
                                         bool handled) {
    if (!isMouseButtonPressed()) {
        return false;
    }
    const auto iterator = mousePressedEvents_.find(static_cast<int>(button));
    if (iterator == mousePressedEvents_.end()) {
        return false;
    }
    const bool result = iterator->second;
    if (result && handled) {
        iterator->second = false;
    }
    return result;
}

bool InputService::getMouseButtonReleased(sf::Mouse::Button button,
                                          bool handled) {
    if (!isMouseButtonReleased()) {
        return false;
    }
    const auto iterator = mouseReleasedEvents_.find(static_cast<int>(button));
    if (iterator == mouseReleasedEvents_.end()) {
        return false;
    }
    const bool result = iterator->second;
    if (result && handled) {
        iterator->second = false;
    }
    return result;
}

bool InputService::isMouseMoved() const {
    return mouseMoved_ && !mouseBlocked_;
}

sf::Vector2i InputService::getMousePosition() const {
    return mousePosition_;
}

std::optional<sf::Vector2i> InputService::getMouseMovedDelta() const {
    return isMouseMoved() ? mouseMovedDelta_ : std::optional<sf::Vector2i>{};
}

void InputService::setMousePosition(const sf::Vector2i& position) {
    if (activeWindow_ == nullptr || !activeWindow_->isOpen()) {
        return;
    }
    const sf::Vector2i pixel = worldToPixel(*activeWindow_, position);
    setMousePosition(pixel, *activeWindow_);
    const sf::Vector2i actualPixel = sf::Mouse::getPosition(*activeWindow_);
    updatePointerViewportState(actualPixel);
    if (!acceptsPointerPixel(actualPixel) && mouseTriggers_.empty()) {
        mousePosition_ = pixelToWorld(*activeWindow_, actualPixel);
        return;
    }
    const sf::Vector2i synchronizedPosition =
        pixelToWorld(*activeWindow_, actualPixel);
    if (synchronizedPosition != mousePosition_) {
        const sf::Vector2i previous = mousePosition_;
        mousePosition_ = synchronizedPosition;
        mouseMoved_ = true;
        mouseMovedDelta_ = synchronizedPosition - previous;
    }
}

void InputService::setMousePosition(const sf::Vector2i& position,
                                    sf::WindowBase& window) {
    if (!ludork::engine::platform_input::setMousePosition(window, position)) {
        sf::Mouse::setPosition(position, window);
    }
}

bool InputService::isMouseEntered() const {
    return mouseEntered_ && !mouseBlocked_;
}

bool InputService::isMouseLeft() const {
    return mouseLeft_ && !mouseBlocked_;
}

bool InputService::isTouchBegan(bool handled) {
    if (touchBlocked_ || !touchBegan_ || touchBeganHandled_) {
        return false;
    }
    if (handled) {
        touchBeganHandled_ = true;
    }
    return true;
}

bool InputService::isTouchTap(bool handled) {
    if (touchBlocked_ || !touchTap_ || touchTapHandled_) {
        return false;
    }
    if (handled) {
        touchTapHandled_ = true;
    }
    return true;
}

bool InputService::isTouchEnded() const {
    return touchEnded_ && !touchBlocked_;
}

bool InputService::isTouchMoved() const {
    return touchMoved_ && !touchBlocked_ && !touchGestureSuppressed_;
}

bool InputService::isTouchDragged() const {
    return touchDragged_ && !touchBlocked_ && !touchGestureSuppressed_;
}

bool InputService::isTouchActive() const {
    return touchActive_ && !touchBlocked_;
}

std::optional<sf::Vector2i> InputService::getTouchPosition() const {
    return touchPosition_;
}

std::optional<sf::Vector2i> InputService::getTouchBeganPosition() const {
    return touchBeganPosition_;
}

std::optional<sf::Vector2i> InputService::getTouchTapPosition() const {
    return touchTapPosition_;
}

std::optional<sf::Vector2i> InputService::getTouchEndedPosition() const {
    return touchEndedPosition_;
}

std::optional<sf::Vector2i> InputService::getTouchMovedDelta() const {
    return isTouchMoved() ? touchMovedDelta_ : std::optional<sf::Vector2i>{};
}

bool InputService::isTouchTriggered(bool handled) {
    if (touchBlocked_ || touchDragged_ || touchGestureSuppressed_ ||
        touchTrigger_.handled || touchTrigger_.count < 1) {
        return false;
    }
    if (handled) {
        touchTrigger_.handled = true;
    }
    return true;
}

bool InputService::isTouchBlocked() const {
    return touchBlocked_;
}

void InputService::blockTouch() {
    abortTwoFingerCancel();
    touchTravelDistance_ = 0.0f;
    touchDragged_ = false;
    touchGestureSuppressed_ = true;
    touchFingers_.clear();
    touchActive_ = false;
    touchTrigger_ = {};
    touchBlocked_ = true;
}

void InputService::unblockTouch() {
    touchBlocked_ = false;
}

bool InputService::isJoystickButtonPressed() const {
    return joystickButtonPressed_ && !joystickBlocked_;
}

bool InputService::isJoystickButtonReleased() const {
    return joystickButtonReleased_ && !joystickBlocked_;
}

bool InputService::isMouseInputMode() const {
    return currentInputType_ == InputType::Mouse;
}

bool InputService::getJoystickButtonPressed(unsigned int joystickId,
                                            unsigned int button, bool handled) {
    if (!isJoystickButtonPressed()) {
        return false;
    }
    const auto joystick = joystickPressedEvents_.find(joystickId);
    if (joystick == joystickPressedEvents_.end()) {
        return false;
    }
    const auto iterator = joystick->second.find(button);
    if (iterator == joystick->second.end()) {
        return false;
    }
    const bool result = iterator->second;
    if (result && handled) {
        iterator->second = false;
    }
    return result;
}

bool InputService::getJoystickButtonValuePressed(unsigned int joystickId,
                                                 const InputNamedValue& button,
                                                 bool handled) {
    return getJoystickButtonPressed(
        joystickId, static_cast<unsigned int>(button.value), handled);
}

bool InputService::getJoystickButtonReleased(unsigned int joystickId,
                                             unsigned int button,
                                             bool handled) {
    if (!isJoystickButtonReleased()) {
        return false;
    }
    const auto joystick = joystickReleasedEvents_.find(joystickId);
    if (joystick == joystickReleasedEvents_.end()) {
        return false;
    }
    const auto iterator = joystick->second.find(button);
    if (iterator == joystick->second.end()) {
        return false;
    }
    const bool result = iterator->second;
    if (result && handled) {
        iterator->second = false;
    }
    return result;
}

bool InputService::getJoystickButtonValueReleased(unsigned int joystickId,
                                                  const InputNamedValue& button,
                                                  bool handled) {
    return getJoystickButtonReleased(
        joystickId, static_cast<unsigned int>(button.value), handled);
}

bool InputService::isJoystickAxisMoved() const {
    return joystickAxisMoved_ && !joystickBlocked_;
}

std::optional<JoystickAxisEvent> InputService::getJoystickAxisMoved(
    unsigned int joystickId, bool handled) {
    if (!isJoystickAxisMoved()) {
        return std::nullopt;
    }
    const auto joystick = joystickAxisEvents_.find(joystickId);
    if (joystick == joystickAxisEvents_.end() || joystick->second.empty()) {
        return std::nullopt;
    }
    const auto axis = joystick->second.begin();
    const JoystickAxisEvent result{axis->first, axis->second};
    if (handled) {
        joystick->second.erase(axis);
        if (joystick->second.empty()) {
            joystickAxisEvents_.erase(joystick);
        }
    }
    return result;
}

bool InputService::isJoystickConnected() const {
    return joystickConnected_ && !joystickBlocked_;
}

bool InputService::isJoystickDisconnected() const {
    return joystickDisconnected_ && !joystickBlocked_;
}

bool InputService::triggerFromMap(
    std::unordered_map<std::string, TriggerEntry>& entries,
    const std::string& id, bool down, bool handled, float repeatDelay,
    float repeatInterval) {
    const auto iterator = entries.find(id);
    if (iterator == entries.end()) {
        return false;
    }
    TriggerEntry& entry = iterator->second;
    if (repeatInterval > 0.0f) {
        const auto now = std::chrono::steady_clock::now();
        if (!entry.handled) {
            entry.repeatStart = now;
            entry.repeatLast = now;
            if (handled) {
                entry.handled = true;
            }
            return true;
        }
        if (down &&
            std::chrono::duration<float>(now - entry.repeatStart).count() >=
                repeatDelay &&
            std::chrono::duration<float>(now - entry.repeatLast).count() >=
                repeatInterval) {
            entry.repeatLast = now;
            return true;
        }
        return false;
    }
    if (entry.handled || entry.count < 1) {
        return false;
    }
    if (handled) {
        entry.handled = true;
    }
    return true;
}

bool InputService::isKeyTriggered(sf::Keyboard::Key key, bool alt, bool ctrl,
                                  bool shift, bool system, bool handled,
                                  float repeatDelay, float repeatInterval) {
    if (!focused_ || keyboardBlocked_) {
        return false;
    }
    return triggerFromMap(
        keyTriggers_, keyId(static_cast<int>(key), {alt, ctrl, shift, system}),
        isKeyboardKeyDown(key), handled, repeatDelay, repeatInterval);
}

bool InputService::isAnyJoystickButtonTriggered(unsigned int button,
                                                bool handled, float repeatDelay,
                                                float repeatInterval) {
    if (joystickBlocked_) {
        return false;
    }
    const auto iterator = joystickButtonTriggers_.find(button);
    if (iterator == joystickButtonTriggers_.end()) {
        return false;
    }
    TriggerEntry& entry = iterator->second;
    if (repeatInterval > 0.0f) {
        const auto now = std::chrono::steady_clock::now();
        if (!entry.handled) {
            entry.repeatStart = now;
            entry.repeatLast = now;
            if (handled) {
                entry.handled = true;
            }
            return true;
        }
        if (isAnyJoystickButtonDown(button) &&
            std::chrono::duration<float>(now - entry.repeatStart).count() >=
                repeatDelay &&
            std::chrono::duration<float>(now - entry.repeatLast).count() >=
                repeatInterval) {
            entry.repeatLast = now;
            return true;
        }
        return false;
    }
    if (entry.handled || entry.count < 1) {
        return false;
    }
    if (handled) {
        entry.handled = true;
    }
    return true;
}

bool InputService::isAnyJoystickButtonValueTriggered(
    const InputNamedValue& button, bool handled, float repeatDelay,
    float repeatInterval) {
    return isAnyJoystickButtonTriggered(static_cast<unsigned int>(button.value),
                                        handled, repeatDelay, repeatInterval);
}

bool InputService::actionTriggered(const InputActionKey& key, bool handled,
                                   float repeatDelay, float repeatInterval) {
    if (key.kind == InputActionKind::KeyOrScan) {
        const bool keyTriggered =
            isKeyTriggered(static_cast<Key>(key.code), false, false, false,
                           false, handled, repeatDelay, repeatInterval);
        bool scanTriggered = false;
        if (focused_ && !keyboardBlocked_) {
            const sf::Keyboard::Scancode scan =
                static_cast<sf::Keyboard::Scancode>(key.code);
            scanTriggered = triggerFromMap(scanTriggers_, keyId(key.code, {}),
                                           isKeyboardScanDown(scan), handled,
                                           repeatDelay, repeatInterval);
        }
        return keyTriggered || scanTriggered;
    }
    if (key.kind == InputActionKind::Key) {
        return isKeyTriggered(static_cast<Key>(key.code), false, false, false,
                              false, handled, repeatDelay, repeatInterval);
    }
    if (key.kind == InputActionKind::Scan) {
        if (!focused_ || keyboardBlocked_) {
            return false;
        }
        const sf::Keyboard::Scancode scan =
            static_cast<sf::Keyboard::Scancode>(key.code);
        return triggerFromMap(scanTriggers_, keyId(key.code, {}),
                              isKeyboardScanDown(scan), handled, repeatDelay,
                              repeatInterval);
    }
    if (key.kind == InputActionKind::JoystickButton) {
        return isAnyJoystickButtonTriggered(static_cast<unsigned int>(key.code),
                                            handled, repeatDelay,
                                            repeatInterval);
    }
    if (key.kind == InputActionKind::MouseButton) {
        return isMouseButtonTriggered(static_cast<sf::Mouse::Button>(key.code),
                                      handled);
    }
    if (key.kind == InputActionKind::TouchTap) {
        return isTouchTap(handled);
    }
    if (joystickBlocked_) {
        return false;
    }
    const sf::Joystick::Axis axis = static_cast<sf::Joystick::Axis>(key.code);
    for (const auto& [joystickId, dominant] : joystickDominantAxis_) {
        if (!dominant.has_value() || *dominant != axis) {
            continue;
        }
        const auto trigger =
            joystickAxisTriggers_.find(axisId(joystickId, axis));
        const auto status = joystickAxisStatus_.find(joystickId);
        if (trigger == joystickAxisTriggers_.end() || trigger->second.handled ||
            trigger->second.count < 1 || status == joystickAxisStatus_.end()) {
            continue;
        }
        const auto position = status->second.find(axis);
        if (position == status->second.end() ||
            !axisMatches(position->second, key)) {
            continue;
        }
        if (handled) {
            trigger->second.handled = true;
        }
        return true;
    }
    if (joystickAxisMoved_ && std::abs(key.threshold) != 10.0f &&
        std::abs(key.threshold) != 50.0f) {
        for (const auto& [joystickId, axes] : joystickAxisEvents_) {
            static_cast<void>(joystickId);
            const auto position = axes.find(axis);
            if (position != axes.end() && axisMatches(position->second, key)) {
                return true;
            }
        }
    }
    return false;
}

bool InputService::isActionTriggered(
    const std::vector<InputActionKey>& actionKeys, bool handled,
    float repeatDelay, float repeatInterval) {
    bool result = false;
    for (const InputActionKey& key : actionKeys) {
        if (actionTriggered(key, handled, repeatDelay, repeatInterval)) {
            result = true;
        }
    }
    return result;
}

bool InputService::actionHeld(const InputActionKey& key) const {
    if (key.kind == InputActionKind::KeyOrScan) {
        return focused_ && !keyboardBlocked_ &&
               (isKeyboardKeyDown(static_cast<Key>(key.code)) ||
                isKeyboardScanDown(
                    static_cast<sf::Keyboard::Scancode>(key.code)));
    }
    if (key.kind == InputActionKind::Key) {
        return focused_ && !keyboardBlocked_ &&
               isKeyboardKeyDown(static_cast<Key>(key.code));
    }
    if (key.kind == InputActionKind::Scan) {
        return focused_ && !keyboardBlocked_ &&
               isKeyboardScanDown(
                   static_cast<sf::Keyboard::Scancode>(key.code));
    }
    if (key.kind == InputActionKind::JoystickButton) {
        return !joystickBlocked_ &&
               isAnyJoystickButtonDown(static_cast<unsigned int>(key.code));
    }
    if (key.kind == InputActionKind::MouseButton) {
        return isMouseButtonDown(static_cast<sf::Mouse::Button>(key.code));
    }
    if (key.kind == InputActionKind::TouchTap) {
        return false;
    }
    if (joystickBlocked_) {
        return false;
    }
    const sf::Joystick::Axis axis = static_cast<sf::Joystick::Axis>(key.code);
    for (const auto& [joystickId, axes] : joystickAxisStatus_) {
        static_cast<void>(joystickId);
        const auto position = axes.find(axis);
        if (position != axes.end() && std::abs(position->second) > 1e-6f &&
            axisMatches(position->second, key)) {
            return true;
        }
    }
    return false;
}

bool InputService::isActionHeld(
    const std::vector<InputActionKey>& actionKeys) const {
    return std::any_of(actionKeys.begin(), actionKeys.end(),
                       [this](const InputActionKey& key) {
                           return actionHeld(key);
                       });
}

bool InputService::isMouseButtonTriggered(sf::Mouse::Button button,
                                          bool handled) {
    if (mouseBlocked_) {
        return false;
    }
    const auto iterator = mouseTriggers_.find(static_cast<int>(button));
    if (iterator == mouseTriggers_.end() || iterator->second.handled ||
        iterator->second.count < 1) {
        return false;
    }
    if (handled) {
        iterator->second.handled = true;
    }
    return true;
}

bool InputService::isMouseButtonDown(sf::Mouse::Button button) const {
    if (mouseBlocked_) {
        return false;
    }
    const int buttonCode = static_cast<int>(button);
    if (pendingMouseTriggerReleases_.contains(buttonCode)) {
        return false;
    }
    const auto iterator = mouseTriggers_.find(buttonCode);
    if (iterator != mouseTriggers_.end() && iterator->second.count >= 1) {
        return true;
    }
    if (useInjectedMouseOnly_ || activeWindow_ == nullptr ||
        !activeWindow_->isOpen() || !activeWindow_->hasFocus() ||
        !pointerInsideViewport_) {
        return false;
    }
    return acceptsPointerPixel(sf::Mouse::getPosition(*activeWindow_)) &&
           sf::Mouse::isButtonPressed(button);
}

std::string InputService::getEnteredText() const {
    return enteredText_;
}

bool InputService::isTextEntered() const {
    return !enteredText_.empty() && !keyboardBlocked_;
}

bool InputService::isKeyboardBlocked() const {
    return keyboardBlocked_;
}

bool InputService::isMouseBlocked() const {
    return mouseBlocked_;
}

bool InputService::isJoystickBlocked() const {
    return joystickBlocked_;
}

void InputService::blockKeyboard() {
    keyboardBlocked_ = true;
}

void InputService::blockMouse() {
    mouseBlocked_ = true;
}

void InputService::blockJoystick() {
    joystickBlocked_ = true;
}

void InputService::unblockKeyboard() {
    keyboardBlocked_ = false;
}

void InputService::unblockMouse() {
    mouseBlocked_ = false;
}

void InputService::unblockJoystick() {
    joystickBlocked_ = false;
}

void InputService::blockInput() {
    keyboardBlocked_ = true;
    mouseBlocked_ = true;
    joystickBlocked_ = true;
    blockTouch();
}

void InputService::unblockInput() {
    keyboardBlocked_ = false;
    mouseBlocked_ = false;
    joystickBlocked_ = false;
    touchBlocked_ = false;
}

std::vector<InputActionKey> InputService::getConfirmKeys() const {
    return {
        keyboardKey(Key::Enter),
        keyboardKey(Key::Space),
        keyboardScan(sf::Keyboard::Scancode::Enter),
        keyboardScan(sf::Keyboard::Scancode::Space),
        joystickButton(JoystickButton::A),
        mouseButton(sf::Mouse::Button::Left),
        touchTap(),
    };
}

std::vector<InputActionKey> InputService::getCancelKeys() const {
    return {
        keyboardKey(Key::Escape),
        keyboardScan(sf::Keyboard::Scancode::Escape),
        joystickButton(JoystickButton::B),
        mouseButton(sf::Mouse::Button::Right),
    };
}

std::vector<InputActionKey> InputService::getUpKeys() const {
    return {
        keyboardKey(Key::Up),
        keyboardScan(sf::Keyboard::Scancode::Up),
        joystickAxis(sf::Joystick::Axis::Y, -10.0f, lessThan),
        joystickAxis(sf::Joystick::Axis::PovY, 50.0f, greaterThan),
    };
}

std::vector<InputActionKey> InputService::getDownKeys() const {
    return {
        keyboardKey(Key::Down),
        keyboardScan(sf::Keyboard::Scancode::Down),
        joystickAxis(sf::Joystick::Axis::Y, 10.0f, greaterThan),
        joystickAxis(sf::Joystick::Axis::PovY, -50.0f, lessThan),
    };
}

std::vector<InputActionKey> InputService::getLeftKeys() const {
    return {
        keyboardKey(Key::Left),
        keyboardScan(sf::Keyboard::Scancode::Left),
        joystickAxis(sf::Joystick::Axis::X, -10.0f, lessThan),
        joystickAxis(sf::Joystick::Axis::PovX, -50.0f, lessThan),
    };
}

std::vector<InputActionKey> InputService::getRightKeys() const {
    return {
        keyboardKey(Key::Right),
        keyboardScan(sf::Keyboard::Scancode::Right),
        joystickAxis(sf::Joystick::Axis::X, 10.0f, greaterThan),
        joystickAxis(sf::Joystick::Axis::PovX, 50.0f, greaterThan),
    };
}

void InputService::registerActionMapping(RuntimeIdentityPtr object,
                                         std::string actionName,
                                         std::vector<InputActionKey> actionKeys,
                                         ActionCallback callback,
                                         bool triggerOnHold) {
    const auto iterator =
        std::find_if(actionMappings_.begin(), actionMappings_.end(),
                     [&actionName, &actionKeys](const ActionMapping& mapping) {
                         return mapping.actionName == actionName &&
                                mapping.actionKeys == actionKeys;
                     });
    ActionMapping mapping{std::move(object), std::move(actionName),
                          std::move(actionKeys), std::move(callback),
                          triggerOnHold};
    if (iterator == actionMappings_.end()) {
        actionMappings_.push_back(std::move(mapping));
    } else {
        *iterator = std::move(mapping);
    }
}

void InputService::unregisterActionMapping(const RuntimeIdentityPtr& object,
                                           const std::string& actionName) {
    std::erase_if(
        actionMappings_, [object, &actionName](const ActionMapping& mapping) {
            const bool sameObject = mapping.object && object
                                        ? mapping.object->equals(*object)
                                        : mapping.object == object;
            return sameObject && mapping.actionName == actionName;
        });
}

void InputService::setFrameCompletionCallback(std::function<void()> callback) {
    frameCompletionCallback_ = std::move(callback);
}

void InputService::shutdown() noexcept {
    ludork::engine::platform_input::shutdown();
    actionMappings_.clear();
    frameCompletionCallback_ = {};
    injectedEvents_.clear();
    resetFrameState();
    clearKeyboardState();
    mouseTriggers_.clear();
    heldKeys_.clear();
    heldScans_.clear();
    abortTwoFingerCancel();
    touchFingers_.clear();
    touchActive_ = false;
    touchPosition_.reset();
    touchTravelDistance_ = 0.0f;
    touchDragged_ = false;
    touchGestureSuppressed_ = false;
    touchTrigger_ = {};
    touchBlocked_ = false;
    touchTapPosition_.reset();
    pointerViewport_.reset();
    pointerInsideViewport_ = true;
    injectedPointerPixel_.reset();
    injectedPointerTransitionPending_.reset();
    joystickAxisStatus_.clear();
    joystickDominantAxis_.clear();
    joystickAxisTriggers_.clear();
    joystickButtonTriggers_.clear();
    activeWindow_ = nullptr;
    FunctionalBase::setInputProvider(nullptr);
}

InputService& inputService() {
    static InputService instance;
    FunctionalBase::setInputProvider(&instance);
    return instance;
}
