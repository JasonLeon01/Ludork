#pragma once

#include <Input/InputAction.hpp>
#include <Input/InputEvent.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Joystick.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class InputRuntime;

using InputActionCallback =
    std::function<void(const RuntimeIdentityPtr&, std::optional<float>)>;

struct InputTriggerEntry {
    int count = 0;
    bool handled = false;
    std::chrono::steady_clock::time_point repeatStart{};
    std::chrono::steady_clock::time_point repeatLast{};
};

struct InputModifiers {
    bool alt = false;
    bool control = false;
    bool shift = false;
    bool system = false;
};

struct HeldKeyState {
    std::size_t physicalCount = 0;
    bool keyOnly = false;
    InputModifiers modifiers;
};

struct HeldScanState {
    sf::Keyboard::Key key = sf::Keyboard::Key::Unknown;
    InputModifiers modifiers;
};

class InputEventPump {
public:
    static void requestSystemCancel() noexcept;

private:
    friend class InputRuntime;

    bool focused_ = true;
    bool focusLost_ = false;
    bool focusGained_ = false;
    InputType currentInputType_ = InputType::Mouse;
    bool useInjectedMouseOnly_ = false;
    std::mutex injectedEventsMutex_;
    std::deque<InjectedInputEvent> injectedEvents_;
    sf::WindowBase* activeWindow_ = nullptr;
    static std::atomic_bool pendingSystemCancel_;
};

class KeyboardInput {
private:
    friend class InputRuntime;

    bool keyPressed_ = false;
    bool keyReleased_ = false;
    std::unordered_map<std::string, bool> keyPressedEvents_;
    std::unordered_map<std::string, bool> keyReleasedEvents_;
    std::unordered_map<std::string, bool> scanPressedEvents_;
    std::unordered_map<std::string, bool> scanReleasedEvents_;
    std::unordered_map<std::string, InputTriggerEntry> keyTriggers_;
    std::unordered_map<std::string, InputTriggerEntry> scanTriggers_;
    std::unordered_map<std::string, std::optional<InputTriggerEntry>>
        keyPulseTriggerBackups_;
    std::unordered_map<std::string, std::optional<InputTriggerEntry>>
        scanPulseTriggerBackups_;
    std::unordered_set<std::string> pendingKeyTriggerReleases_;
    std::unordered_set<std::string> pendingScanTriggerReleases_;
    std::unordered_map<int, HeldKeyState> heldKeys_;
    std::unordered_map<int, HeldScanState> heldScans_;
    std::string enteredText_;
    bool blocked_ = false;
};

class PointerInput {
private:
    friend class InputRuntime;

    bool mouseWheelScrolled_ = false;
    std::optional<sf::Mouse::Wheel> mouseWheel_;
    float mouseWheelDelta_ = 0.0f;
    bool mouseWheelPrecise_ = false;
    std::optional<sf::Vector2i> mouseWheelPosition_;
    bool mouseButtonPressed_ = false;
    bool mouseButtonReleased_ = false;
    std::unordered_map<int, bool> mousePressedEvents_;
    std::unordered_map<int, bool> mouseReleasedEvents_;
    std::unordered_map<int, InputTriggerEntry> mouseTriggers_;
    std::unordered_set<int> pendingMouseTriggerReleases_;
    bool mouseMoved_ = false;
    sf::Vector2i mousePosition_ = {0, 0};
    std::optional<sf::Vector2i> mouseMovedDelta_;
    bool mouseEntered_ = false;
    bool mouseLeft_ = false;
    std::optional<sf::IntRect> viewport_;
    bool insideViewport_ = true;
    std::optional<sf::Vector2i> injectedPixel_;
    std::optional<bool> injectedTransitionPending_;
    bool touchBegan_ = false;
    bool touchEnded_ = false;
    bool touchMoved_ = false;
    bool touchActive_ = false;
    std::optional<sf::Vector2i> touchPosition_;
    std::optional<sf::Vector2i> touchBeganPosition_;
    bool touchTap_ = false;
    bool touchTapHandled_ = false;
    std::optional<sf::Vector2i> touchTapPosition_;
    std::optional<sf::Vector2i> touchEndedPosition_;
    std::optional<sf::Vector2i> touchMovedDelta_;
    bool touchBeganHandled_ = false;
    InputTriggerEntry touchTrigger_;
    float touchTravelDistance_ = 0.0f;
    bool touchDragged_ = false;
    bool touchGestureSuppressed_ = false;
    std::optional<unsigned int> primaryTouchFinger_;
    std::unordered_map<unsigned int, sf::Vector2i> touchFingers_;
    bool touchCancelMouseActive_ = false;
    bool touchCancelMousePressedThisFrame_ = false;
    std::optional<sf::Vector2i> touchCancelMouseReleasePending_;
    bool mouseBlocked_ = false;
    bool touchBlocked_ = false;
};

class JoystickInput {
private:
    friend class InputRuntime;

    bool buttonPressed_ = false;
    bool buttonReleased_ = false;
    bool axisMoved_ = false;
    bool connected_ = false;
    bool disconnected_ = false;
    std::unordered_map<unsigned int, std::unordered_map<unsigned int, bool>>
        pressedEvents_;
    std::unordered_map<unsigned int, std::unordered_map<unsigned int, bool>>
        releasedEvents_;
    std::unordered_map<unsigned int,
                       std::unordered_map<sf::Joystick::Axis, float>>
        axisEvents_;
    std::unordered_map<unsigned int,
                       std::unordered_map<sf::Joystick::Axis, float>>
        axisStatus_;
    std::unordered_map<unsigned int, std::optional<sf::Joystick::Axis>>
        dominantAxis_;
    std::unordered_map<std::string, InputTriggerEntry> axisTriggers_;
    std::unordered_map<unsigned int, InputTriggerEntry> buttonTriggers_;
    bool blocked_ = false;
};

struct InputActionMapping {
    RuntimeIdentityPtr object;
    std::string actionName;
    std::vector<InputActionKey> actionKeys;
    InputActionCallback callback;
    bool triggerOnHold = false;
};

class InputActions {
private:
    friend class InputRuntime;

    std::vector<InputActionMapping> mappings_;
};

class InputRuntime {
public:
    void initializeNativePolling();
    void update(sf::WindowBase& window);
    void injectEvent(const InjectedInputEvent& event);
    void setUseInjectedMouseOnly(bool value);
    void setPointerViewport(std::optional<sf::IntRect> viewport);
    void onWindowRecreated(sf::WindowBase& window);

    bool isFocused() const;
    bool isFocusLost() const;
    bool isFocusGained() const;
    bool isKeyPressed() const;
    bool isKeyReleased() const;
    bool getKeyPressed(sf::Keyboard::Key key, bool handled, bool alt, bool ctrl,
                       bool shift, bool system);
    bool getScanPressed(sf::Keyboard::Scancode scan, bool handled, bool alt,
                        bool ctrl, bool shift, bool system);
    bool getKeyReleased(sf::Keyboard::Key key, bool handled, bool alt,
                        bool ctrl, bool shift, bool system);
    bool getScanReleased(sf::Keyboard::Scancode scan, bool handled, bool alt,
                         bool ctrl, bool shift, bool system);

    bool isMouseWheelScrolled() const;
    std::optional<sf::Mouse::Wheel> getMouseScrolledWheel() const;
    float getMouseScrolledWheelDelta() const;
    bool isMouseWheelPrecise() const;
    std::optional<sf::Vector2i> getMouseScrolledWheelPosition() const;
    bool isMouseButtonPressed() const;
    bool isMouseButtonReleased() const;
    bool getMouseButtonPressed(sf::Mouse::Button button, bool handled);
    bool getMouseButtonReleased(sf::Mouse::Button button, bool handled);
    bool isMouseMoved() const;
    sf::Vector2i getMousePosition() const;
    std::optional<sf::Vector2i> getMouseMovedDelta() const;
    void setMousePosition(const sf::Vector2i& position);
    void setMousePosition(const sf::Vector2i& position, sf::WindowBase& window);
    bool isMouseEntered() const;
    bool isMouseLeft() const;

    bool isTouchBegan(bool handled);
    bool isTouchTap(bool handled);
    bool isTouchEnded() const;
    bool isTouchMoved() const;
    bool isTouchDragged() const;
    bool isTouchActive() const;
    std::optional<sf::Vector2i> getTouchPosition() const;
    std::optional<sf::Vector2i> getTouchBeganPosition() const;
    std::optional<sf::Vector2i> getTouchTapPosition() const;
    std::optional<sf::Vector2i> getTouchEndedPosition() const;
    std::optional<sf::Vector2i> getTouchMovedDelta() const;
    void cancelTouchGesture() noexcept;
    bool isTouchTriggered(bool handled);
    bool isTouchBlocked() const;
    void blockTouch();
    void unblockTouch();

    bool isJoystickButtonPressed() const;
    bool isJoystickButtonReleased() const;
    bool isMouseInputMode() const;
    bool getJoystickButtonPressed(unsigned int joystickId, unsigned int button,
                                  bool handled);
    bool getJoystickButtonValuePressed(unsigned int joystickId,
                                       const InputNamedValue& button,
                                       bool handled);
    bool getJoystickButtonReleased(unsigned int joystickId, unsigned int button,
                                   bool handled);
    bool getJoystickButtonValueReleased(unsigned int joystickId,
                                        const InputNamedValue& button,
                                        bool handled);
    bool isJoystickAxisMoved() const;
    std::optional<JoystickAxisEvent> getJoystickAxisMoved(
        unsigned int joystickId, bool handled);
    bool isJoystickConnected() const;
    bool isJoystickDisconnected() const;

    bool isKeyTriggered(sf::Keyboard::Key key, bool alt, bool ctrl, bool shift,
                        bool system, bool handled, float repeatDelay,
                        float repeatInterval);
    bool isAnyJoystickButtonTriggered(unsigned int button, bool handled,
                                      float repeatDelay, float repeatInterval);
    bool isAnyJoystickButtonValueTriggered(const InputNamedValue& button,
                                           bool handled, float repeatDelay,
                                           float repeatInterval);
    bool isActionTriggered(const std::vector<InputActionKey>& actionKeys,
                           bool handled, float repeatDelay,
                           float repeatInterval);
    bool isActionHeld(const std::vector<InputActionKey>& actionKeys) const;
    bool isMouseButtonTriggered(sf::Mouse::Button button, bool handled);
    bool isMouseButtonDown(sf::Mouse::Button button) const;

    std::string getEnteredText() const;
    bool isTextEntered() const;
    bool isKeyboardBlocked() const;
    bool isMouseBlocked() const;
    bool isJoystickBlocked() const;
    void blockKeyboard();
    void blockMouse();
    void blockJoystick();
    void unblockKeyboard();
    void unblockMouse();
    void unblockJoystick();
    void blockInput();
    void unblockInput();

    std::vector<InputActionKey> getConfirmKeys() const;
    std::vector<InputActionKey> getCancelKeys() const;
    std::vector<InputActionKey> getUpKeys() const;
    std::vector<InputActionKey> getDownKeys() const;
    std::vector<InputActionKey> getLeftKeys() const;
    std::vector<InputActionKey> getRightKeys() const;
    void registerActionMapping(RuntimeIdentityPtr object,
                               std::string actionName,
                               std::vector<InputActionKey> actionKeys,
                               InputActionCallback callback,
                               bool triggerOnHold);
    void unregisterActionMapping(const RuntimeIdentityPtr& object,
                                 const std::string& actionName);

    void setFrameCompletionCallback(std::function<void()> callback);
    void shutdown() noexcept;

private:
    static std::string keyId(int code, const InputModifiers& modifiers);
    static std::string axisId(unsigned int joystickId, sf::Joystick::Axis axis);
    static bool axisMatches(float position, const InputActionKey& key);
    static sf::Keyboard::Key keyFromName(const std::string& name);
    static sf::Mouse::Button mouseButtonFromName(const std::string& name);
    static std::string toUtf8(char32_t codepoint);
    static sf::Vector2i pixelToWorld(sf::WindowBase& window,
                                     const sf::Vector2i& pixel);
    static sf::Vector2i worldToPixel(sf::WindowBase& window,
                                     const sf::Vector2i& position);

    void resetFrameState();
    void consumePendingSystemCancel();
    void restoreKeyPulses();
    void clearKeyboardState();
    void setFocused(bool focused);
    void setKeyPressed(sf::Keyboard::Key key, sf::Keyboard::Scancode scan,
                       const InputModifiers& modifiers);
    void setKeyReleased(sf::Keyboard::Key key, sf::Keyboard::Scancode scan,
                        const InputModifiers& modifiers);
    void setKeyPulse(sf::Keyboard::Key key, sf::Keyboard::Scancode scan,
                     const InputModifiers& modifiers);
    void setMouseButtonPressed(sf::Mouse::Button button,
                               const sf::Vector2i& position);
    void setMouseButtonReleased(sf::Mouse::Button button,
                                const sf::Vector2i& position);
    void beginTwoFingerCancel(const sf::Vector2i& position);
    void endTwoFingerCancel(const sf::Vector2i& position);
    void releasePendingTwoFingerCancel();
    void abortTwoFingerCancel();
    void recordMouseWheel(sf::Mouse::Wheel wheel, float delta,
                          const sf::Vector2i& position, bool precise = false);
    bool acceptsPointerPixel(const sf::Vector2i& pixel) const;
    void updatePointerViewportState(bool inside);
    void updatePointerViewportState(const sf::Vector2i& pixel);
    void processPlatformScrollEvents(sf::WindowBase& window);
    void processInjectedEvents();
    bool processNativeEvent(sf::WindowBase& window, const sf::Event& event);
    void updateJoystickDominantAxes();
    void updateInputType(sf::WindowBase& window);
    void dispatchActionMappings();
    bool triggerFromMap(
        std::unordered_map<std::string, InputTriggerEntry>& entries,
        const std::string& id, bool down, bool handled, float repeatDelay,
        float repeatInterval);
    bool consume(std::unordered_map<std::string, bool>& events,
                 const std::string& id, bool handled);
    bool actionTriggered(const InputActionKey& key, bool handled,
                         float repeatDelay, float repeatInterval);
    bool actionHeld(const InputActionKey& key) const;
    bool isKeyboardKeyDown(sf::Keyboard::Key key) const;
    bool isKeyboardScanDown(sf::Keyboard::Scancode scan) const;
    bool isAnyJoystickButtonDown(unsigned int button) const;

    InputEventPump eventPump_;
    KeyboardInput keyboard_;
    PointerInput pointer_;
    JoystickInput joystick_;
    InputActions actions_;
    std::function<void()> frameCompletionCallback_;
};

InputRuntime& inputRuntime();
