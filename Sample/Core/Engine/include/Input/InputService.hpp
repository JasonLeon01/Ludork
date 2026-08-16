#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <UI/FunctionalBase.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Joystick.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/WindowBase.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class InputType {
    Mouse = 0,
    Gamepad = 1
};

enum class JoystickButton : unsigned int {
    A = 0,
    B = 1,
    X = 2,
    Y = 3,
    LB = 4,
    RB = 5,
    View = 6,
    Menu = 7,
    LS = 8,
    RS = 9,
    XBox = 10,
    Share = 11
};

using InputAxisComparison = std::function<bool(float, float)>;

enum class InputActionKind {
    KeyOrScan,
    Key,
    Scan,
    MouseButton,
    JoystickButton,
    JoystickAxis,
    TouchTap
};

BIND_CLASS(copyable = true, table_init = true,
           metadata = false, lua_alternatives = "integer=>value=$",
           lua_tostring = "name")
struct InputNamedValue {
    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    int value = 0;
};

BIND_MODULE_PROPERTY(name = "JoystickButton", metadata = false,
                     reverse = "Input.JoyStickButtonName", cache = true)
extern LUDORK_ENGINE_API const std::unordered_map<std::string, InputNamedValue>
    inputJoystickButtons;

BIND_MODULE_PROPERTY(name = "InputType", metadata = false,
                     cache = true)
extern LUDORK_ENGINE_API const std::unordered_map<std::string, InputNamedValue>
    inputTypes;

BIND_MODULE_PROPERTY(name = "ActionKind", metadata = false)
extern LUDORK_ENGINE_API const std::unordered_map<std::string, int>
    inputActionKinds;

BIND_MODULE_PROPERTY(name = "AxisComparison", metadata = false)
extern LUDORK_ENGINE_API const
    std::unordered_map<std::string, InputAxisComparison>
        inputAxisComparisons;

BIND_CLASS(copyable = true, table_init = true,
    lua_alternatives =
        "number=>kind=InputActionKind::KeyOrScan,code=$;fields(name,value)=>"
        "kind=InputActionKind::JoystickButton,name=$name,code=$value;array("
        "axis,threshold,comparison)=>kind=InputActionKind::JoystickAxis,code=$"
        "axis,threshold=$threshold,comparison=$comparison,comparisonIdentity=$"
        "comparison",
    lua_emit =
        "kind=InputActionKind::KeyOrScan=>value($code);kind=InputActionKind::"
        "Key=>fields(kind=$kind,code=$code);kind=InputActionKind::Scan=>fields("
        "kind=$kind,code=$code);kind=InputActionKind::JoystickButton=>fields("
        "name=$name,value=$code);kind=InputActionKind::JoystickAxis=>array($"
        "code,$threshold,$comparison)")
struct InputActionKey {
    BIND_PROPERTY()
    InputActionKind kind = InputActionKind::KeyOrScan;

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    int code = 0;

    BIND_PROPERTY()
    float threshold = 0.0f;

    BIND_PROPERTY(metadata = false)
    InputAxisComparison comparison;

    BIND_PROPERTY(metadata = false)
    RuntimeIdentityPtr comparisonIdentity;

    bool operator==(const InputActionKey& other) const;
};

BIND_CLASS(copyable = true, table_init = true)
struct InjectedInputEvent {
    BIND_PROPERTY()
    std::string type;

    BIND_PROPERTY()
    std::string key;

    BIND_PROPERTY()
    int scan = static_cast<int>(sf::Keyboard::Scancode::Unknown);

    BIND_PROPERTY()
    std::string button = "Left";

    BIND_PROPERTY()
    int x = 0;

    BIND_PROPERTY()
    int y = 0;

    BIND_PROPERTY()
    float delta = 0.0f;

    BIND_PROPERTY()
    bool alt = false;

    BIND_PROPERTY()
    bool control = false;

    BIND_PROPERTY()
    bool shift = false;

    BIND_PROPERTY()
    bool system = false;
};

BIND_CLASS(copyable = true)
struct JoystickAxisEvent {
    BIND_PROPERTY()
    sf::Joystick::Axis axis = sf::Joystick::Axis::X;

    BIND_PROPERTY()
    float position = 0.0f;
};

class LUDORK_ENGINE_API InputService;
LUDORK_ENGINE_API InputService& inputService();

BIND_CLASS(name = "Service", module = "Input",
           singleton = "inputService", bind_bases = false)
class LUDORK_ENGINE_API InputService : public FunctionalInputProvider {
public:
    using ActionCallback =
        std::function<void(const RuntimeIdentityPtr&, std::optional<float>)>;

    void initializeNativePolling();

    BIND_METHOD()
    void update(sf::WindowBase& window);

    BIND_METHOD()
    void injectEvent(const InjectedInputEvent& event);

    BIND_METHOD()
    void setUseInjectedMouseOnly(bool value);

    BIND_IGNORE()
    void setPointerViewport(std::optional<sf::IntRect> viewport);

    BIND_IGNORE()
    void onWindowRecreated(sf::WindowBase& window);

    BIND_IGNORE()
    static void requestSystemCancel() noexcept;

    BIND_METHOD(Pure = true)
    bool isFocused() const;

    BIND_METHOD(Pure = true)
    bool isFocusLost() const;

    BIND_METHOD(Pure = true)
    bool isFocusGained() const;

    BIND_METHOD(Pure = true)
    bool isKeyPressed() const override;

    BIND_METHOD(Pure = true)
    bool isKeyReleased() const override;

    BIND_METHOD()
    bool getKeyPressed(sf::Keyboard::Key key, bool handled, bool alt = false,
                       bool ctrl = false, bool shift = false,
                       bool system = false);

    BIND_METHOD()
    bool getScanPressed(sf::Keyboard::Scancode scan, bool handled,
                        bool alt = false, bool ctrl = false, bool shift = false,
                        bool system = false);

    BIND_METHOD()
    bool getKeyReleased(sf::Keyboard::Key key, bool handled, bool alt = false,
                        bool ctrl = false, bool shift = false,
                        bool system = false);

    BIND_METHOD()
    bool getScanReleased(sf::Keyboard::Scancode scan, bool handled,
                         bool alt = false, bool ctrl = false,
                         bool shift = false, bool system = false);

    BIND_METHOD(Pure = true)
    bool isMouseWheelScrolled() const override;

    BIND_METHOD(Pure = true)
    std::optional<sf::Mouse::Wheel> getMouseScrolledWheel() const;

    BIND_METHOD(Pure = true)
    float getMouseScrolledWheelDelta() const override;

    BIND_METHOD(Pure = true)
    bool isMouseWheelPrecise() const;

    BIND_METHOD(Pure = true)
    std::optional<sf::Vector2i> getMouseScrolledWheelPosition() const;

    BIND_METHOD(Pure = true)
    bool isMouseButtonPressed() const override;

    BIND_METHOD(Pure = true)
    bool isMouseButtonReleased() const override;

    BIND_METHOD()
    bool getMouseButtonPressed(sf::Mouse::Button button, bool handled) override;

    BIND_METHOD()
    bool getMouseButtonReleased(sf::Mouse::Button button,
                                bool handled) override;

    BIND_METHOD(Pure = true)
    bool isMouseMoved() const override;

    BIND_METHOD(Pure = true)
    sf::Vector2i getMousePosition() const override;

    BIND_METHOD(Pure = true)
    std::optional<sf::Vector2i> getMouseMovedDelta() const;

    BIND_METHOD()
    void setMousePosition(const sf::Vector2i& position);

    void setMousePosition(const sf::Vector2i& position, sf::WindowBase& window);

    BIND_METHOD(Pure = true)
    bool isMouseEntered() const;

    BIND_METHOD(Pure = true)
    bool isMouseLeft() const;

    BIND_METHOD()
    bool isTouchBegan(bool handled = false) override;

    BIND_METHOD()
    bool isTouchTap(bool handled = false) override;

    BIND_METHOD(Pure = true)
    bool isTouchEnded() const override;

    BIND_METHOD(Pure = true)
    bool isTouchMoved() const override;

    BIND_METHOD(Pure = true)
    bool isTouchDragged() const;

    BIND_METHOD(Pure = true)
    bool isTouchActive() const override;

    BIND_METHOD(Pure = true)
    std::optional<sf::Vector2i> getTouchPosition() const override;

    BIND_METHOD(Pure = true)
    std::optional<sf::Vector2i> getTouchBeganPosition() const override;

    BIND_METHOD(Pure = true)
    std::optional<sf::Vector2i> getTouchTapPosition() const override;

    BIND_METHOD(Pure = true)
    std::optional<sf::Vector2i> getTouchEndedPosition() const override;

    BIND_METHOD(Pure = true)
    std::optional<sf::Vector2i> getTouchMovedDelta() const;

    BIND_METHOD()
    void cancelTouchGesture() noexcept override;

    BIND_METHOD()
    bool isTouchTriggered(bool handled = false);

    BIND_METHOD(Pure = true)
    bool isTouchBlocked() const;

    BIND_METHOD()
    void blockTouch();

    BIND_METHOD()
    void unblockTouch();

    BIND_METHOD(Pure = true)
    bool isJoystickButtonPressed() const override;

    BIND_METHOD(Pure = true)
    bool isJoystickButtonReleased() const override;

    BIND_METHOD(Pure = true)
    bool isMouseInputMode() const override;

    BIND_METHOD()
    bool getJoystickButtonPressed(unsigned int joystickId, unsigned int button,
                                  bool handled);

    BIND_METHOD(name = "getJoystickButtonPressed", metadata = false)
    bool getJoystickButtonValuePressed(unsigned int joystickId,
                                       const InputNamedValue& button,
                                       bool handled);

    BIND_METHOD()
    bool getJoystickButtonReleased(unsigned int joystickId, unsigned int button,
                                   bool handled);

    BIND_METHOD(name = "getJoystickButtonReleased", metadata = false)
    bool getJoystickButtonValueReleased(unsigned int joystickId,
                                        const InputNamedValue& button,
                                        bool handled);

    BIND_METHOD(Pure = true)
    bool isJoystickAxisMoved() const override;

    BIND_METHOD()
    std::optional<JoystickAxisEvent> getJoystickAxisMoved(
        unsigned int joystickId, bool handled);

    BIND_METHOD(Pure = true)
    bool isJoystickConnected() const;

    BIND_METHOD(Pure = true)
    bool isJoystickDisconnected() const;

    BIND_METHOD()
    bool isKeyTriggered(sf::Keyboard::Key key, bool alt = false,
                        bool ctrl = false, bool shift = false,
                        bool system = false, bool handled = false,
                        float repeatDelay = 0.0f, float repeatInterval = 0.0f);

    BIND_METHOD()
    bool isAnyJoystickButtonTriggered(unsigned int button, bool handled = false,
                                      float repeatDelay = 0.0f,
                                      float repeatInterval = 0.0f);

    BIND_METHOD(name = "isAnyJoystickButtonTriggered", metadata = false)
    bool isAnyJoystickButtonValueTriggered(const InputNamedValue& button,
                                           bool handled = false,
                                           float repeatDelay = 0.0f,
                                           float repeatInterval = 0.0f);

    BIND_METHOD()
    bool isActionTriggered(const std::vector<InputActionKey>& actionKeys,
                           bool handled = false, float repeatDelay = 0.0f,
                           float repeatInterval = 0.0f);

    BIND_METHOD(Pure = true)
    bool isActionHeld(const std::vector<InputActionKey>& actionKeys) const;

    BIND_METHOD()
    bool isMouseButtonTriggered(sf::Mouse::Button button,
                                bool handled = false) override;

    BIND_METHOD(Pure = true)
    bool isMouseButtonDown(sf::Mouse::Button button) const override;

    BIND_METHOD(Pure = true)
    std::string getEnteredText() const;

    BIND_METHOD(Pure = true)
    bool isTextEntered() const;

    BIND_METHOD(Pure = true)
    bool isKeyboardBlocked() const;

    BIND_METHOD(Pure = true)
    bool isMouseBlocked() const;

    BIND_METHOD(Pure = true)
    bool isJoystickBlocked() const;

    BIND_METHOD()
    void blockKeyboard();

    BIND_METHOD()
    void blockMouse();

    BIND_METHOD()
    void blockJoystick();

    BIND_METHOD()
    void unblockKeyboard();

    BIND_METHOD()
    void unblockMouse();

    BIND_METHOD()
    void unblockJoystick();

    BIND_METHOD()
    void blockInput();

    BIND_METHOD()
    void unblockInput();

    BIND_METHOD(Pure = true)
    std::vector<InputActionKey> getConfirmKeys() const;

    BIND_METHOD(Pure = true)
    std::vector<InputActionKey> getCancelKeys() const;

    BIND_METHOD(Pure = true)
    std::vector<InputActionKey> getUpKeys() const;

    BIND_METHOD(Pure = true)
    std::vector<InputActionKey> getDownKeys() const;

    BIND_METHOD(Pure = true)
    std::vector<InputActionKey> getLeftKeys() const;

    BIND_METHOD(Pure = true)
    std::vector<InputActionKey> getRightKeys() const;

    BIND_METHOD()
    void registerActionMapping(
        RuntimeIdentityPtr object, std::string actionName,
        std::vector<InputActionKey> actionKeys,
        std::function<void(const RuntimeIdentityPtr&, std::optional<float>)>
            callback,
        bool triggerOnHold = false);

    BIND_METHOD()
    void unregisterActionMapping(const RuntimeIdentityPtr& object,
                                 const std::string& actionName);

    void setFrameCompletionCallback(std::function<void()> callback);

    void shutdown() noexcept;

private:
    friend LUDORK_ENGINE_API InputService& inputService();
    InputService() = default;

    struct TriggerEntry {
        int count = 0;
        bool handled = false;
        std::chrono::steady_clock::time_point repeatStart{};
        std::chrono::steady_clock::time_point repeatLast{};
    };

    struct Modifiers {
        bool alt = false;
        bool control = false;
        bool shift = false;
        bool system = false;
    };

    struct ActionMapping {
        RuntimeIdentityPtr object;
        std::string actionName;
        std::vector<InputActionKey> actionKeys;
        ActionCallback callback;
        bool triggerOnHold = false;
    };

    static std::string keyId(int code, const Modifiers& modifiers);
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
    void clearKeyboardState();
    void recordMouseWheel(sf::Mouse::Wheel wheel, float delta,
                          const sf::Vector2i& position, bool precise = false);
    void processPlatformScrollEvents(sf::WindowBase& window);
    bool acceptsPointerPixel(const sf::Vector2i& pixel) const;
    void updatePointerViewportState(bool inside);
    void updatePointerViewportState(const sf::Vector2i& pixel);
    void processInjectedEvents();
    bool processNativeEvent(sf::WindowBase& window, const sf::Event& event);
    void setFocused(bool focused);
    void setKeyPressed(sf::Keyboard::Key key, sf::Keyboard::Scancode scan,
                       const Modifiers& modifiers);
    void setKeyReleased(sf::Keyboard::Key key, sf::Keyboard::Scancode scan,
                        const Modifiers& modifiers);
    void setMouseButtonPressed(sf::Mouse::Button button,
                               const sf::Vector2i& position);
    void setMouseButtonReleased(sf::Mouse::Button button,
                                const sf::Vector2i& position);
    void beginTwoFingerCancel(const sf::Vector2i& position);
    void endTwoFingerCancel(const sf::Vector2i& position);
    void releasePendingTwoFingerCancel();
    void abortTwoFingerCancel();
    void updateJoystickDominantAxes();
    void updateInputType(sf::WindowBase& window);
    void dispatchActionMappings();
    bool triggerFromMap(std::unordered_map<std::string, TriggerEntry>& entries,
                        const std::string& id, bool down, bool handled,
                        float repeatDelay, float repeatInterval);
    bool consume(std::unordered_map<std::string, bool>& events,
                 const std::string& id, bool handled);
    bool actionTriggered(const InputActionKey& key, bool handled,
                         float repeatDelay, float repeatInterval);
    bool actionHeld(const InputActionKey& key) const;
    bool isKeyboardKeyDown(sf::Keyboard::Key key) const;
    bool isKeyboardScanDown(sf::Keyboard::Scancode scan) const;
    bool isAnyJoystickButtonDown(unsigned int button) const;

    bool focused_ = true;
    bool focusLost_ = false;
    bool focusGained_ = false;
    InputType currentInputType_ = InputType::Mouse;
    bool keyPressed_ = false;
    bool keyReleased_ = false;
    std::unordered_map<std::string, bool> keyPressedEvents_;
    std::unordered_map<std::string, bool> keyReleasedEvents_;
    std::unordered_map<std::string, bool> scanPressedEvents_;
    std::unordered_map<std::string, bool> scanReleasedEvents_;
    std::unordered_map<std::string, TriggerEntry> keyTriggers_;
    std::unordered_map<std::string, TriggerEntry> scanTriggers_;
    std::unordered_set<std::string> pendingKeyTriggerReleases_;
    std::unordered_set<std::string> pendingScanTriggerReleases_;
    std::unordered_set<int> heldKeys_;
    std::unordered_set<int> heldScans_;
    bool mouseWheelScrolled_ = false;
    std::optional<sf::Mouse::Wheel> mouseWheel_;
    float mouseWheelDelta_ = 0.0f;
    bool mouseWheelPrecise_ = false;
    std::optional<sf::Vector2i> mouseWheelPosition_;
    bool mouseButtonPressed_ = false;
    bool mouseButtonReleased_ = false;
    std::unordered_map<int, bool> mousePressedEvents_;
    std::unordered_map<int, bool> mouseReleasedEvents_;
    std::unordered_map<int, TriggerEntry> mouseTriggers_;
    std::unordered_set<int> pendingMouseTriggerReleases_;
    bool mouseMoved_ = false;
    sf::Vector2i mousePosition_ = {0, 0};
    std::optional<sf::Vector2i> mouseMovedDelta_;
    bool mouseEntered_ = false;
    bool mouseLeft_ = false;
    std::optional<sf::IntRect> pointerViewport_;
    bool pointerInsideViewport_ = true;
    std::optional<sf::Vector2i> injectedPointerPixel_;
    std::optional<bool> injectedPointerTransitionPending_;
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
    TriggerEntry touchTrigger_;
    float touchTravelDistance_ = 0.0f;
    bool touchDragged_ = false;
    bool touchGestureSuppressed_ = false;
    std::optional<unsigned int> primaryTouchFinger_;
    std::unordered_map<unsigned int, sf::Vector2i> touchFingers_;
    bool touchCancelMouseActive_ = false;
    bool touchCancelMousePressedThisFrame_ = false;
    std::optional<sf::Vector2i> touchCancelMouseReleasePending_;
    bool joystickButtonPressed_ = false;
    bool joystickButtonReleased_ = false;
    bool joystickAxisMoved_ = false;
    bool joystickConnected_ = false;
    bool joystickDisconnected_ = false;
    std::unordered_map<unsigned int, std::unordered_map<unsigned int, bool>>
        joystickPressedEvents_;
    std::unordered_map<unsigned int, std::unordered_map<unsigned int, bool>>
        joystickReleasedEvents_;
    std::unordered_map<unsigned int,
                       std::unordered_map<sf::Joystick::Axis, float>>
        joystickAxisEvents_;
    std::unordered_map<unsigned int,
                       std::unordered_map<sf::Joystick::Axis, float>>
        joystickAxisStatus_;
    std::unordered_map<unsigned int, std::optional<sf::Joystick::Axis>>
        joystickDominantAxis_;
    std::unordered_map<std::string, TriggerEntry> joystickAxisTriggers_;
    std::unordered_map<unsigned int, TriggerEntry> joystickButtonTriggers_;
    std::string enteredText_;
    bool keyboardBlocked_ = false;
    bool mouseBlocked_ = false;
    bool joystickBlocked_ = false;
    bool touchBlocked_ = false;
    bool useInjectedMouseOnly_ = false;
    std::deque<InjectedInputEvent> injectedEvents_;
    std::vector<ActionMapping> actionMappings_;
    std::function<void()> frameCompletionCallback_;
    sf::WindowBase* activeWindow_ = nullptr;
    static std::atomic_bool pendingSystemCancel_;
};

BIND_FUNCTION()
LUDORK_ENGINE_API InputService& inputService();

BIND_LUA_REVERSE(path = "Input.KeyName", source = "sf.Keyboard.Key")
BIND_LUA_REVERSE(path = "Input.ScanName", source = "sf.Keyboard.Scan")
BIND_LUA_REVERSE(path = "Input.JoystickAxisName", source = "sf.Joystick.Axis")
