#include "InputRuntime.hpp"

#include <algorithm>

namespace {

bool isKnownKey(sf::Keyboard::Key key) {
    return key != sf::Keyboard::Key::Unknown;
}

bool isKnownScan(sf::Keyboard::Scancode scan) {
    return scan != sf::Keyboard::Scancode::Unknown;
}

void installPulseTrigger(
    std::unordered_map<std::string, InputTriggerEntry>& triggers,
    std::unordered_map<std::string, std::optional<InputTriggerEntry>>& backups,
    const std::string& identifier) {
    const auto [backup, inserted] = backups.try_emplace(identifier);
    if (inserted) {
        const auto trigger = triggers.find(identifier);
        if (trigger != triggers.end()) {
            backup->second = trigger->second;
        }
    }
    triggers[identifier] = InputTriggerEntry{1};
}

void recordTriggerPress(
    std::unordered_map<std::string, InputTriggerEntry>& triggers,
    std::unordered_map<std::string, std::optional<InputTriggerEntry>>& backups,
    const std::string& identifier) {
    const auto backup = backups.find(identifier);
    if (backup == backups.end()) {
        ++triggers[identifier].count;
        return;
    }
    if (!backup->second.has_value()) {
        backup->second = InputTriggerEntry{};
    }
    ++backup->second->count;
}

void restorePulseTriggers(
    std::unordered_map<std::string, InputTriggerEntry>& triggers,
    std::unordered_map<std::string, std::optional<InputTriggerEntry>>&
        backups) {
    for (const auto& [identifier, trigger] : backups) {
        if (trigger.has_value()) {
            triggers[identifier] = *trigger;
        } else {
            triggers.erase(identifier);
        }
    }
    backups.clear();
}

}  // namespace

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
    keyboard_.keyPulseTriggerBackups_.clear();
    keyboard_.scanPulseTriggerBackups_.clear();
    keyboard_.pendingKeyTriggerReleases_.clear();
    keyboard_.pendingScanTriggerReleases_.clear();
    keyboard_.heldKeys_.clear();
    keyboard_.heldScans_.clear();
}

void InputRuntime::setKeyPressed(sf::Keyboard::Key key,
                                 sf::Keyboard::Scancode scan,
                                 const InputModifiers& modifiers) {
    const bool knownKey = isKnownKey(key);
    const bool knownScan = isKnownScan(scan);
    if (!knownKey && !knownScan) {
        return;
    }

    const int keyCode = static_cast<int>(key);
    const int scanCode = static_cast<int>(scan);
    bool keyPressed = false;
    bool scanPressed = false;
    if (knownScan) {
        if (keyboard_.heldScans_.contains(scanCode)) {
            return;
        }
        if (knownKey) {
            const auto heldKey = keyboard_.heldKeys_.find(keyCode);
            if (heldKey != keyboard_.heldKeys_.end() &&
                heldKey->second.keyOnly) {
                return;
            }
            if (heldKey == keyboard_.heldKeys_.end()) {
                keyboard_.heldKeys_.emplace(keyCode,
                                            HeldKeyState{1, false, modifiers});
                keyPressed = true;
            } else {
                ++heldKey->second.physicalCount;
            }
        }
        keyboard_.heldScans_.emplace(scanCode, HeldScanState{key, modifiers});
        scanPressed = true;
    } else {
        if (keyboard_.heldKeys_.contains(keyCode)) {
            return;
        }
        keyboard_.heldKeys_.emplace(keyCode, HeldKeyState{1, true, modifiers});
        keyPressed = true;
    }

    keyboard_.keyPressed_ = true;
    if (keyPressed) {
        const std::string identifier = keyId(keyCode, modifiers);
        keyboard_.pendingKeyTriggerReleases_.erase(identifier);
        keyboard_.keyPressedEvents_[identifier] = true;
        recordTriggerPress(keyboard_.keyTriggers_,
                           keyboard_.keyPulseTriggerBackups_, identifier);
    }
    if (scanPressed) {
        const std::string identifier = keyId(scanCode, modifiers);
        keyboard_.pendingScanTriggerReleases_.erase(identifier);
        keyboard_.scanPressedEvents_[identifier] = true;
        recordTriggerPress(keyboard_.scanTriggers_,
                           keyboard_.scanPulseTriggerBackups_, identifier);
    }
}

void InputRuntime::setKeyReleased(sf::Keyboard::Key key,
                                  sf::Keyboard::Scancode scan,
                                  const InputModifiers& modifiers) {
    const bool knownKey = isKnownKey(key);
    const bool knownScan = isKnownScan(scan);
    if (!knownKey && !knownScan) {
        return;
    }

    bool keyReleased = false;
    bool scanReleased = false;
    int releasedKeyCode = static_cast<int>(sf::Keyboard::Key::Unknown);
    int releasedScanCode = static_cast<int>(sf::Keyboard::Scancode::Unknown);
    InputModifiers keyPressModifiers;
    InputModifiers scanPressModifiers;

    auto releaseHeldKey = [&](int keyCode) {
        const auto heldKey = keyboard_.heldKeys_.find(keyCode);
        if (heldKey == keyboard_.heldKeys_.end()) {
            return;
        }
        keyPressModifiers = heldKey->second.modifiers;
        releasedKeyCode = keyCode;
        keyReleased = true;
        keyboard_.heldKeys_.erase(heldKey);
    };

    auto releaseHeldScan = [&](auto heldScan) {
        releasedScanCode = heldScan->first;
        scanPressModifiers = heldScan->second.modifiers;
        scanReleased = true;
        const sf::Keyboard::Key heldKeyValue = heldScan->second.key;
        keyboard_.heldScans_.erase(heldScan);
        if (!isKnownKey(heldKeyValue)) {
            return;
        }
        const int heldKeyCode = static_cast<int>(heldKeyValue);
        const auto heldKey = keyboard_.heldKeys_.find(heldKeyCode);
        if (heldKey == keyboard_.heldKeys_.end() || heldKey->second.keyOnly) {
            return;
        }
        if (heldKey->second.physicalCount > 1) {
            --heldKey->second.physicalCount;
            return;
        }
        releaseHeldKey(heldKeyCode);
    };

    if (knownScan) {
        const auto heldScan = keyboard_.heldScans_.find(static_cast<int>(scan));
        if (heldScan != keyboard_.heldScans_.end()) {
            releaseHeldScan(heldScan);
        } else if (knownKey) {
            const auto heldKey =
                keyboard_.heldKeys_.find(static_cast<int>(key));
            if (heldKey != keyboard_.heldKeys_.end() &&
                heldKey->second.keyOnly) {
                releaseHeldKey(heldKey->first);
            }
        }
    } else if (knownKey) {
        const int keyCode = static_cast<int>(key);
        const auto heldKey = keyboard_.heldKeys_.find(keyCode);
        if (heldKey != keyboard_.heldKeys_.end() && heldKey->second.keyOnly) {
            releaseHeldKey(keyCode);
        } else if (heldKey != keyboard_.heldKeys_.end() &&
                   heldKey->second.physicalCount == 1) {
            const auto heldScan = std::find_if(
                keyboard_.heldScans_.begin(), keyboard_.heldScans_.end(),
                [key](const auto& entry) {
                    return entry.second.key == key;
                });
            if (heldScan != keyboard_.heldScans_.end()) {
                releaseHeldScan(heldScan);
            }
        }
    }

    if (!keyReleased && !scanReleased) {
        return;
    }
    keyboard_.keyReleased_ = true;
    if (keyReleased) {
        const std::string releaseIdentifier = keyId(releasedKeyCode, modifiers);
        const std::string pressIdentifier =
            keyId(releasedKeyCode, keyPressModifiers);
        keyboard_.keyReleasedEvents_[releaseIdentifier] = true;
        if (keyboard_.keyPressedEvents_.contains(pressIdentifier)) {
            keyboard_.pendingKeyTriggerReleases_.insert(pressIdentifier);
        } else {
            keyboard_.keyTriggers_.erase(pressIdentifier);
            keyboard_.pendingKeyTriggerReleases_.erase(pressIdentifier);
        }
    }
    if (scanReleased) {
        const std::string releaseIdentifier =
            keyId(releasedScanCode, modifiers);
        const std::string pressIdentifier =
            keyId(releasedScanCode, scanPressModifiers);
        keyboard_.scanReleasedEvents_[releaseIdentifier] = true;
        if (keyboard_.scanPressedEvents_.contains(pressIdentifier)) {
            keyboard_.pendingScanTriggerReleases_.insert(pressIdentifier);
        } else {
            keyboard_.scanTriggers_.erase(pressIdentifier);
            keyboard_.pendingScanTriggerReleases_.erase(pressIdentifier);
        }
    }
}

void InputRuntime::setKeyPulse(sf::Keyboard::Key key,
                               sf::Keyboard::Scancode scan,
                               const InputModifiers& modifiers) {
    const bool knownKey = isKnownKey(key);
    const bool knownScan = isKnownScan(scan);
    if (!knownKey && !knownScan) {
        return;
    }

    keyboard_.keyPressed_ = true;
    keyboard_.keyReleased_ = true;
    if (knownKey) {
        const std::string identifier = keyId(static_cast<int>(key), modifiers);
        keyboard_.keyPressedEvents_[identifier] = true;
        keyboard_.keyReleasedEvents_[identifier] = true;
        installPulseTrigger(keyboard_.keyTriggers_,
                            keyboard_.keyPulseTriggerBackups_, identifier);
    }
    if (knownScan) {
        const std::string identifier = keyId(static_cast<int>(scan), modifiers);
        keyboard_.scanPressedEvents_[identifier] = true;
        keyboard_.scanReleasedEvents_[identifier] = true;
        installPulseTrigger(keyboard_.scanTriggers_,
                            keyboard_.scanPulseTriggerBackups_, identifier);
    }
}

void InputRuntime::restoreKeyPulses() {
    restorePulseTriggers(keyboard_.keyTriggers_,
                         keyboard_.keyPulseTriggerBackups_);
    restorePulseTriggers(keyboard_.scanTriggers_,
                         keyboard_.scanPulseTriggerBackups_);
}

bool InputRuntime::isKeyboardKeyDown(sf::Keyboard::Key key) const {
    if (!isKnownKey(key)) {
        return false;
    }
    if (keyboard_.heldKeys_.contains(static_cast<int>(key))) {
        return true;
    }
    return !eventPump_.useInjectedMouseOnly_ && sf::Keyboard::isKeyPressed(key);
}

bool InputRuntime::isKeyboardScanDown(sf::Keyboard::Scancode scan) const {
    if (!isKnownScan(scan)) {
        return false;
    }
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
