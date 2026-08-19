#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Input/InputAction.hpp>
#include <Input/InputEvent.hpp>
#include <Input/InputProvider.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/WindowBase.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

class LUDORK_ENGINE_API InputService;
LUDORK_ENGINE_API InputService& inputService();

BIND_CLASS(name = "Service", module = "Input", singleton = "inputService",
           bind_bases = false)
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
};

BIND_FUNCTION()
LUDORK_ENGINE_API InputService& inputService();

BIND_LUA_REVERSE(path = "Input.KeyName", source = "sf.Keyboard.Key")
BIND_LUA_REVERSE(path = "Input.ScanName", source = "sf.Keyboard.Scan")
BIND_LUA_REVERSE(path = "Input.JoystickAxisName", source = "sf.Joystick.Axis")
