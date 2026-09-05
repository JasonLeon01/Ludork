#include "InputRuntime.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

using Key = sf::Keyboard::Key;

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

InputActionKey joystickButton(const InputNamedValue& button) {
    InputActionKey result;
    result.kind = InputActionKind::JoystickButton;
    result.name = button.name;
    result.code = button.value;
    return result;
}

InputActionKey joystickAxis(sf::Joystick::Axis axis, float threshold,
                            const std::string& comparisonName) {
    InputActionKey result;
    result.kind = InputActionKind::JoystickAxis;
    result.code = static_cast<int>(axis);
    result.threshold = threshold;
    result.comparison = inputAxisComparisons.at(comparisonName);
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

bool isDefaultDirectionalAxis(const InputActionKey& key) {
    if (key.kind != InputActionKind::JoystickAxis) {
        return false;
    }
    const sf::Joystick::Axis axis = static_cast<sf::Joystick::Axis>(key.code);
    const float magnitude = std::abs(key.threshold);
    if (axis == sf::Joystick::Axis::X || axis == sf::Joystick::Axis::Y) {
        return std::abs(magnitude - 10.0f) < 1e-6f;
    }
    if (axis == sf::Joystick::Axis::PovX || axis == sf::Joystick::Axis::PovY) {
        return std::abs(magnitude - 50.0f) < 1e-6f;
    }
    return false;
}

}  // namespace

bool InputRuntime::axisMatches(float position, const InputActionKey& key) {
    return key.comparison ? key.comparison(position, key.threshold)
                          : position > key.threshold;
}

void InputRuntime::dispatchActionMappings() {
    struct MoveAction {
        float position = 0.0f;
        InputActionCallback callback;
        RuntimeIdentityPtr object;
    };
    std::vector<MoveAction> moveActions;
    const std::vector<InputActionMapping> mappings = actions_.mappings_;
    for (const InputActionMapping& mapping : mappings) {
        if (!mapping.callback) {
            continue;
        }
        for (const InputActionKey& key : mapping.actionKeys) {
            if (key.kind == InputActionKind::JoystickButton) {
                if (joystick_.blocked_) {
                    continue;
                }
                bool triggered = false;
                for (const auto& [joystickId, buttons] :
                     joystick_.pressedEvents_) {
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
                if (joystick_.blocked_) {
                    continue;
                }
                const sf::Joystick::Axis axis =
                    static_cast<sf::Joystick::Axis>(key.code);
                for (const auto& [joystickId, axes] : joystick_.axisStatus_) {
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
                if (!pointer_.mouseBlocked_ &&
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
            } else if (eventPump_.focused_ && !keyboard_.blocked_) {
                bool triggered = false;
                const InputModifiers modifiers{};
                if (key.kind == InputActionKind::Scan) {
                    const std::string identifier = keyId(key.code, modifiers);
                    const auto iterator =
                        keyboard_.scanPressedEvents_.find(identifier);
                    triggered =
                        iterator != keyboard_.scanPressedEvents_.end() &&
                        iterator->second;
                    if (!triggered && mapping.triggerOnHold) {
                        triggered = isKeyboardScanDown(
                            static_cast<sf::Keyboard::Scancode>(key.code));
                    }
                } else if (key.kind == InputActionKind::Key) {
                    const std::string identifier = keyId(key.code, modifiers);
                    const auto iterator =
                        keyboard_.keyPressedEvents_.find(identifier);
                    triggered = iterator != keyboard_.keyPressedEvents_.end() &&
                                iterator->second;
                    if (!triggered && mapping.triggerOnHold) {
                        triggered = isKeyboardKeyDown(
                            static_cast<sf::Keyboard::Key>(key.code));
                    }
                } else if (key.kind == InputActionKind::KeyOrScan) {
                    const std::string identifier = keyId(key.code, modifiers);
                    const auto keyIterator =
                        keyboard_.keyPressedEvents_.find(identifier);
                    const auto scanIterator =
                        keyboard_.scanPressedEvents_.find(identifier);
                    const bool keyTriggered =
                        keyIterator != keyboard_.keyPressedEvents_.end() &&
                        keyIterator->second;
                    const bool scanTriggered =
                        scanIterator != keyboard_.scanPressedEvents_.end() &&
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

bool InputRuntime::triggerFromMap(
    std::unordered_map<std::string, InputTriggerEntry>& entries,
    const std::string& id, bool down, bool handled, float repeatDelay,
    float repeatInterval) {
    const auto iterator = entries.find(id);
    if (iterator == entries.end()) {
        return false;
    }
    InputTriggerEntry& entry = iterator->second;
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

bool InputRuntime::isKeyTriggered(sf::Keyboard::Key key, bool alt, bool ctrl,
                                  bool shift, bool system, bool handled,
                                  float repeatDelay, float repeatInterval) {
    if (!eventPump_.focused_ || keyboard_.blocked_) {
        return false;
    }
    return triggerFromMap(
        keyboard_.keyTriggers_,
        keyId(static_cast<int>(key), {alt, ctrl, shift, system}),
        isKeyboardKeyDown(key), handled, repeatDelay, repeatInterval);
}

bool InputRuntime::isAnyJoystickButtonTriggered(unsigned int button,
                                                bool handled, float repeatDelay,
                                                float repeatInterval) {
    if (joystick_.blocked_) {
        return false;
    }
    const auto iterator = joystick_.buttonTriggers_.find(button);
    if (iterator == joystick_.buttonTriggers_.end()) {
        return false;
    }
    InputTriggerEntry& entry = iterator->second;
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

bool InputRuntime::isAnyJoystickButtonValueTriggered(
    const InputNamedValue& button, bool handled, float repeatDelay,
    float repeatInterval) {
    return isAnyJoystickButtonTriggered(static_cast<unsigned int>(button.value),
                                        handled, repeatDelay, repeatInterval);
}

bool InputRuntime::actionTriggered(const InputActionKey& key, bool handled,
                                   float repeatDelay, float repeatInterval) {
    if (key.kind == InputActionKind::KeyOrScan) {
        const bool keyTriggered =
            isKeyTriggered(static_cast<Key>(key.code), false, false, false,
                           false, handled, repeatDelay, repeatInterval);
        bool scanTriggered = false;
        if (eventPump_.focused_ && !keyboard_.blocked_) {
            const sf::Keyboard::Scancode scan =
                static_cast<sf::Keyboard::Scancode>(key.code);
            scanTriggered = triggerFromMap(
                keyboard_.scanTriggers_, keyId(key.code, {}),
                isKeyboardScanDown(scan), handled, repeatDelay, repeatInterval);
        }
        return keyTriggered || scanTriggered;
    }
    if (key.kind == InputActionKind::Key) {
        return isKeyTriggered(static_cast<Key>(key.code), false, false, false,
                              false, handled, repeatDelay, repeatInterval);
    }
    if (key.kind == InputActionKind::Scan) {
        if (!eventPump_.focused_ || keyboard_.blocked_) {
            return false;
        }
        const sf::Keyboard::Scancode scan =
            static_cast<sf::Keyboard::Scancode>(key.code);
        return triggerFromMap(keyboard_.scanTriggers_, keyId(key.code, {}),
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
    if (joystick_.blocked_) {
        return false;
    }
    const sf::Joystick::Axis axis = static_cast<sf::Joystick::Axis>(key.code);
    for (const auto& [joystickId, dominant] : joystick_.dominantAxis_) {
        if (!dominant.has_value() || *dominant != axis) {
            continue;
        }
        const auto trigger =
            joystick_.axisTriggers_.find(axisId(joystickId, axis));
        const auto status = joystick_.axisStatus_.find(joystickId);
        if (trigger == joystick_.axisTriggers_.end() ||
            trigger->second.handled || trigger->second.count < 1 ||
            status == joystick_.axisStatus_.end()) {
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
    if (joystick_.axisMoved_ && !isDefaultDirectionalAxis(key)) {
        for (const auto& [joystickId, axes] : joystick_.axisEvents_) {
            static_cast<void>(joystickId);
            const auto position = axes.find(axis);
            if (position != axes.end() && axisMatches(position->second, key)) {
                return true;
            }
        }
    }
    return false;
}

bool InputRuntime::isActionTriggered(
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

bool InputRuntime::actionHeld(const InputActionKey& key) const {
    if (key.kind == InputActionKind::KeyOrScan) {
        return eventPump_.focused_ && !keyboard_.blocked_ &&
               (isKeyboardKeyDown(static_cast<Key>(key.code)) ||
                isKeyboardScanDown(
                    static_cast<sf::Keyboard::Scancode>(key.code)));
    }
    if (key.kind == InputActionKind::Key) {
        return eventPump_.focused_ && !keyboard_.blocked_ &&
               isKeyboardKeyDown(static_cast<Key>(key.code));
    }
    if (key.kind == InputActionKind::Scan) {
        return eventPump_.focused_ && !keyboard_.blocked_ &&
               isKeyboardScanDown(
                   static_cast<sf::Keyboard::Scancode>(key.code));
    }
    if (key.kind == InputActionKind::JoystickButton) {
        return !joystick_.blocked_ &&
               isAnyJoystickButtonDown(static_cast<unsigned int>(key.code));
    }
    if (key.kind == InputActionKind::MouseButton) {
        return isMouseButtonDown(static_cast<sf::Mouse::Button>(key.code));
    }
    if (key.kind == InputActionKind::TouchTap) {
        return false;
    }
    if (joystick_.blocked_) {
        return false;
    }
    const sf::Joystick::Axis axis = static_cast<sf::Joystick::Axis>(key.code);
    if (isDefaultDirectionalAxis(key)) {
        for (const auto& [joystickId, dominant] : joystick_.dominantAxis_) {
            if (!dominant.has_value() || *dominant != axis) {
                continue;
            }
            const auto status = joystick_.axisStatus_.find(joystickId);
            if (status == joystick_.axisStatus_.end()) {
                continue;
            }
            const auto position = status->second.find(axis);
            if (position != status->second.end() &&
                axisMatches(position->second, key)) {
                return true;
            }
        }
        return false;
    }
    for (const auto& [joystickId, axes] : joystick_.axisStatus_) {
        static_cast<void>(joystickId);
        const auto position = axes.find(axis);
        if (position != axes.end() && std::abs(position->second) > 1e-6f &&
            axisMatches(position->second, key)) {
            return true;
        }
    }
    return false;
}

bool InputRuntime::isActionHeld(
    const std::vector<InputActionKey>& actionKeys) const {
    return std::any_of(actionKeys.begin(), actionKeys.end(),
                       [this](const InputActionKey& key) {
                           return actionHeld(key);
                       });
}

std::vector<InputActionKey> InputRuntime::getConfirmKeys() const {
    return {
        keyboardKey(Key::Enter),
        keyboardKey(Key::Space),
        keyboardScan(sf::Keyboard::Scancode::Enter),
        keyboardScan(sf::Keyboard::Scancode::Space),
        joystickButton(JoystickButton::getA()),
        mouseButton(sf::Mouse::Button::Left),
        touchTap(),
    };
}

std::vector<InputActionKey> InputRuntime::getCancelKeys() const {
    return {
        keyboardKey(Key::Escape),
        keyboardScan(sf::Keyboard::Scancode::Escape),
        joystickButton(JoystickButton::getB()),
        mouseButton(sf::Mouse::Button::Right),
    };
}

std::vector<InputActionKey> InputRuntime::getUpKeys() const {
    return {
        keyboardKey(Key::Up),
        keyboardScan(sf::Keyboard::Scancode::Up),
        joystickAxis(sf::Joystick::Axis::Y, -10.0f, "Less"),
        joystickAxis(sf::Joystick::Axis::PovY, 50.0f, "Greater"),
    };
}

std::vector<InputActionKey> InputRuntime::getDownKeys() const {
    return {
        keyboardKey(Key::Down),
        keyboardScan(sf::Keyboard::Scancode::Down),
        joystickAxis(sf::Joystick::Axis::Y, 10.0f, "Greater"),
        joystickAxis(sf::Joystick::Axis::PovY, -50.0f, "Less"),
    };
}

std::vector<InputActionKey> InputRuntime::getLeftKeys() const {
    return {
        keyboardKey(Key::Left),
        keyboardScan(sf::Keyboard::Scancode::Left),
        joystickAxis(sf::Joystick::Axis::X, -10.0f, "Less"),
        joystickAxis(sf::Joystick::Axis::PovX, -50.0f, "Less"),
    };
}

std::vector<InputActionKey> InputRuntime::getRightKeys() const {
    return {
        keyboardKey(Key::Right),
        keyboardScan(sf::Keyboard::Scancode::Right),
        joystickAxis(sf::Joystick::Axis::X, 10.0f, "Greater"),
        joystickAxis(sf::Joystick::Axis::PovX, 50.0f, "Greater"),
    };
}

void InputRuntime::registerActionMapping(RuntimeIdentityPtr object,
                                         std::string actionName,
                                         std::vector<InputActionKey> actionKeys,
                                         InputActionCallback callback,
                                         bool triggerOnHold) {
    const auto iterator = std::find_if(
        actions_.mappings_.begin(), actions_.mappings_.end(),
        [&actionName, &actionKeys](const InputActionMapping& mapping) {
            return mapping.actionName == actionName &&
                   mapping.actionKeys == actionKeys;
        });
    InputActionMapping mapping{std::move(object), std::move(actionName),
                               std::move(actionKeys), std::move(callback),
                               triggerOnHold};
    if (iterator == actions_.mappings_.end()) {
        actions_.mappings_.push_back(std::move(mapping));
    } else {
        *iterator = std::move(mapping);
    }
}

void InputRuntime::unregisterActionMapping(const RuntimeIdentityPtr& object,
                                           const std::string& actionName) {
    std::erase_if(actions_.mappings_,
                  [object, &actionName](const InputActionMapping& mapping) {
                      const bool sameObject =
                          mapping.object && object
                              ? mapping.object->equals(*object)
                              : mapping.object == object;
                      return sameObject && mapping.actionName == actionName;
                  });
}
