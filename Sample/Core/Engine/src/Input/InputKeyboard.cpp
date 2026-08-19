#include "InputRuntime.hpp"

std::string InputRuntime::keyId(int code, const InputModifiers& modifiers) {
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

void InputRuntime::clearKeyboardState() {
    keyboard_.keyPressed_ = false;
    keyboard_.keyReleased_ = false;
    keyboard_.keyPressedEvents_.clear();
    keyboard_.keyReleasedEvents_.clear();
    keyboard_.scanPressedEvents_.clear();
    keyboard_.scanReleasedEvents_.clear();
    keyboard_.keyTriggers_.clear();
    keyboard_.scanTriggers_.clear();
    keyboard_.pendingKeyTriggerReleases_.clear();
    keyboard_.pendingScanTriggerReleases_.clear();
    keyboard_.heldKeys_.clear();
    keyboard_.heldScans_.clear();
}

void InputRuntime::setKeyPressed(sf::Keyboard::Key key,
                                 sf::Keyboard::Scancode scan,
                                 const InputModifiers& modifiers) {
    keyboard_.keyPressed_ = true;
    const std::string keyIdentifier = keyId(static_cast<int>(key), modifiers);
    const std::string scanIdentifier = keyId(static_cast<int>(scan), modifiers);
    keyboard_.pendingKeyTriggerReleases_.erase(keyIdentifier);
    keyboard_.pendingScanTriggerReleases_.erase(scanIdentifier);
    keyboard_.keyPressedEvents_[keyIdentifier] = true;
    keyboard_.scanPressedEvents_[scanIdentifier] = true;
    keyboard_.heldKeys_.insert(static_cast<int>(key));
    keyboard_.heldScans_.insert(static_cast<int>(scan));
    InputTriggerEntry& keyEntry = keyboard_.keyTriggers_[keyIdentifier];
    ++keyEntry.count;
    InputTriggerEntry& scanEntry = keyboard_.scanTriggers_[scanIdentifier];
    ++scanEntry.count;
}

void InputRuntime::setKeyReleased(sf::Keyboard::Key key,
                                  sf::Keyboard::Scancode scan,
                                  const InputModifiers& modifiers) {
    keyboard_.keyReleased_ = true;
    const std::string keyIdentifier = keyId(static_cast<int>(key), modifiers);
    const std::string scanIdentifier = keyId(static_cast<int>(scan), modifiers);
    keyboard_.keyReleasedEvents_[keyIdentifier] = true;
    keyboard_.scanReleasedEvents_[scanIdentifier] = true;
    if (keyboard_.keyPressedEvents_.contains(keyIdentifier)) {
        keyboard_.pendingKeyTriggerReleases_.insert(keyIdentifier);
    } else {
        keyboard_.keyTriggers_.erase(keyIdentifier);
        keyboard_.pendingKeyTriggerReleases_.erase(keyIdentifier);
    }
    if (keyboard_.scanPressedEvents_.contains(scanIdentifier)) {
        keyboard_.pendingScanTriggerReleases_.insert(scanIdentifier);
    } else {
        keyboard_.scanTriggers_.erase(scanIdentifier);
        keyboard_.pendingScanTriggerReleases_.erase(scanIdentifier);
    }
    keyboard_.heldKeys_.erase(static_cast<int>(key));
    keyboard_.heldScans_.erase(static_cast<int>(scan));
}

bool InputRuntime::isKeyboardKeyDown(sf::Keyboard::Key key) const {
    if (keyboard_.heldKeys_.contains(static_cast<int>(key))) {
        return true;
    }
    return !eventPump_.useInjectedMouseOnly_ && sf::Keyboard::isKeyPressed(key);
}

bool InputRuntime::isKeyboardScanDown(sf::Keyboard::Scancode scan) const {
    if (keyboard_.heldScans_.contains(static_cast<int>(scan))) {
        return true;
    }
    return !eventPump_.useInjectedMouseOnly_ &&
           sf::Keyboard::isKeyPressed(scan);
}

bool InputRuntime::isKeyPressed() const {
    return keyboard_.keyPressed_ && eventPump_.focused_ && !keyboard_.blocked_;
}

bool InputRuntime::isKeyReleased() const {
    return keyboard_.keyReleased_ && eventPump_.focused_ && !keyboard_.blocked_;
}

bool InputRuntime::consume(std::unordered_map<std::string, bool>& events,
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

bool InputRuntime::getKeyPressed(sf::Keyboard::Key key, bool handled, bool alt,
                                 bool ctrl, bool shift, bool system) {
    if (!isKeyPressed()) {
        return false;
    }
    const InputModifiers modifiers{alt, ctrl, shift, system};
    const std::string identifier = keyId(static_cast<int>(key), modifiers);
    const auto direct = keyboard_.keyPressedEvents_.find(identifier);
    if (direct != keyboard_.keyPressedEvents_.end()) {
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
    const bool result =
        consume(keyboard_.scanPressedEvents_,
                keyId(static_cast<int>(scan), modifiers), handled);
    if (result && handled) {
        const auto unknown = keyboard_.keyPressedEvents_.find(
            keyId(static_cast<int>(sf::Keyboard::Key::Unknown), modifiers));
        if (unknown != keyboard_.keyPressedEvents_.end()) {
            unknown->second = false;
        }
    }
    return result;
}

bool InputRuntime::getScanPressed(sf::Keyboard::Scancode scan, bool handled,
                                  bool alt, bool ctrl, bool shift,
                                  bool system) {
    if (!isKeyPressed()) {
        return false;
    }
    return consume(keyboard_.scanPressedEvents_,
                   keyId(static_cast<int>(scan), {alt, ctrl, shift, system}),
                   handled);
}

bool InputRuntime::getKeyReleased(sf::Keyboard::Key key, bool handled, bool alt,
                                  bool ctrl, bool shift, bool system) {
    if (!isKeyReleased()) {
        return false;
    }
    const InputModifiers modifiers{alt, ctrl, shift, system};
    const std::string identifier = keyId(static_cast<int>(key), modifiers);
    const auto direct = keyboard_.keyReleasedEvents_.find(identifier);
    if (direct != keyboard_.keyReleasedEvents_.end()) {
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
    const bool result =
        consume(keyboard_.scanReleasedEvents_,
                keyId(static_cast<int>(scan), modifiers), handled);
    if (result && handled) {
        const auto unknown = keyboard_.keyReleasedEvents_.find(
            keyId(static_cast<int>(sf::Keyboard::Key::Unknown), modifiers));
        if (unknown != keyboard_.keyReleasedEvents_.end()) {
            unknown->second = false;
        }
    }
    return result;
}

bool InputRuntime::getScanReleased(sf::Keyboard::Scancode scan, bool handled,
                                   bool alt, bool ctrl, bool shift,
                                   bool system) {
    if (!isKeyReleased()) {
        return false;
    }
    return consume(keyboard_.scanReleasedEvents_,
                   keyId(static_cast<int>(scan), {alt, ctrl, shift, system}),
                   handled);
}

std::string InputRuntime::getEnteredText() const {
    return keyboard_.enteredText_;
}

bool InputRuntime::isTextEntered() const {
    return !keyboard_.enteredText_.empty() && !keyboard_.blocked_;
}

bool InputRuntime::isKeyboardBlocked() const {
    return keyboard_.blocked_;
}

void InputRuntime::blockKeyboard() {
    keyboard_.blocked_ = true;
}

void InputRuntime::unblockKeyboard() {
    keyboard_.blocked_ = false;
}
