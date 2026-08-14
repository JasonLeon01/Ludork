#pragma once

#include <BindAnnotations.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <UI/FocusableMixin.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>

#include <functional>
#include <optional>
#include <string>

class FunctionalInputProvider {
public:
    virtual ~FunctionalInputProvider() = default;
    virtual sf::Vector2i getMousePosition() const = 0;
    virtual bool isMouseButtonPressed() const = 0;
    virtual bool getMouseButtonPressed(sf::Mouse::Button button,
                                       bool handled) = 0;
    virtual bool isMouseButtonTriggered(sf::Mouse::Button button,
                                        bool handled) = 0;
    virtual bool isMouseMoved() const = 0;
    virtual bool isMouseWheelScrolled() const = 0;
    virtual float getMouseScrolledWheelDelta() const = 0;
    virtual bool isMouseInputMode() const = 0;
    virtual bool isTouchBegan(bool handled) = 0;
    virtual bool isTouchTap(bool handled) = 0;
    virtual bool isTouchMoved() const = 0;
    virtual std::optional<sf::Vector2i> getTouchBeganPosition() const = 0;
    virtual std::optional<sf::Vector2i> getTouchTapPosition() const = 0;
    virtual std::optional<sf::Vector2i> getTouchPosition() const = 0;
    virtual bool isMouseButtonReleased() const = 0;
    virtual bool getMouseButtonReleased(sf::Mouse::Button button,
                                        bool handled) = 0;
    virtual bool isMouseButtonDown(sf::Mouse::Button button) const = 0;
    virtual bool isTouchEnded() const = 0;
    virtual bool isTouchActive() const = 0;
    virtual std::optional<sf::Vector2i> getTouchEndedPosition() const = 0;
    virtual void cancelTouchGesture() noexcept = 0;
    virtual bool isKeyPressed() const = 0;
    virtual bool isKeyReleased() const = 0;
    virtual bool isJoystickButtonPressed() const = 0;
    virtual bool isJoystickButtonReleased() const = 0;
    virtual bool isJoystickAxisMoved() const = 0;
};

BIND_CLASS(callbacks =
               "onConfirm,onCancel,onClick,onMouseButtonDown,onHover,onUnHover,"
               "onMouseMoved,onMouseWheelScrolled,onKeyDown,onKeyUp,onTick,"
               "onLateTick,onFixedTick")
class LUDORK_ENGINE_API FunctionalBase : public FocusableMixin {
public:
    using FocusResolver = std::function<bool(const FunctionalBase&)>;
    using DirectionalFocusRequester =
        std::function<bool(FunctionalBase&, const std::string&)>;
    using FocusSetter = std::function<bool(FunctionalBase&)>;
    using EventCallback =
        std::function<void(FunctionalBase&, const RuntimeValue::Map&)>;
    using HandledEventCallback =
        std::function<bool(FunctionalBase&, const RuntimeValue::Map&)>;

    BIND_INIT()
    FunctionalBase() = default;
    virtual ~FunctionalBase();

    static void setInputProvider(FunctionalInputProvider* provider);

    BIND_METHOD()
    static void setKeyboardFocusResolver(
        std::function<bool(const FunctionalBase&)> resolver);

    BIND_METHOD()
    static void setDirectionalFocusRequester(
        std::function<bool(FunctionalBase&, const std::string&)> requester);

    BIND_METHOD()
    static void setKeyboardFocusSetter(
        std::function<bool(FunctionalBase&)> setter);

    BIND_METHOD()
    static void setKeyboardCursorResolver(
        std::function<bool(const FunctionalBase&)> resolver);

    static void resetRuntimeCallbacks() noexcept;

    BIND_METHOD(Pure = true)
    bool canReceiveFocus() const;

    BIND_METHOD(Pure = true)
    bool shouldDispatchKeyboardInput() const;

    BIND_METHOD()
    bool requestDirectionalFocusMove(const std::string& direction);

    BIND_METHOD()
    bool requestKeyboardFocus();

    BIND_METHOD(Pure = true)
    bool ownsKeyboardCursorFocus() const;

    BIND_METHOD(Pure = true)
    bool isHovered() const;

    BIND_METHOD(Pure = true)
    bool isPressed() const;

    BIND_METHOD(Pure = true)
    bool getActive() const;

    BIND_METHOD()
    void setActive(bool active);

    BIND_METHOD()
    void setTouchHitBounds(
        const std::optional<sf::FloatRect>& bounds);

    BIND_METHOD(Pure = true)
    std::optional<sf::FloatRect> getAbsoluteTouchHitBounds() const;

    BIND_METHOD()
    void addConfirmCallback(
        std::function<void(FunctionalBase&, const RuntimeValue::Map&)>
            callback);

    BIND_METHOD()
    void addCancelCallback(
        std::function<void(FunctionalBase&, const RuntimeValue::Map&)>
            callback);

    BIND_METHOD()
    void addClickCallback(
        std::function<void(FunctionalBase&, const RuntimeValue::Map&)>
            callback);

    BIND_METHOD()
    void addMouseButtonDownCallback(
        std::function<bool(FunctionalBase&, const RuntimeValue::Map&)>
            callback);

    BIND_METHOD()
    void addHoverCallback(
        std::function<void(FunctionalBase&, const RuntimeValue::Map&)>
            callback);

    BIND_METHOD()
    void addUnHoverCallback(
        std::function<void(FunctionalBase&, const RuntimeValue::Map&)>
            callback);

    BIND_METHOD()
    void addMouseMovedCallback(
        std::function<void(FunctionalBase&, const RuntimeValue::Map&)>
            callback);

    BIND_METHOD()
    void addMouseWheelScrolledCallback(
        std::function<void(FunctionalBase&, const RuntimeValue::Map&)>
            callback);

    BIND_METHOD()
    void addKeyDownCallback(
        std::function<void(FunctionalBase&, const RuntimeValue::Map&)>
            callback);

    BIND_METHOD()
    void addKeyUpCallback(
        std::function<void(FunctionalBase&, const RuntimeValue::Map&)>
            callback);

    BIND_METHOD()
    virtual void update(float deltaTime);

    BIND_METHOD()
    virtual void lateUpdate(float deltaTime);

    BIND_METHOD()
    virtual void fixedUpdate(float fixedDelta);

    BIND_METHOD()
    virtual void onConfirm(const RuntimeValue::Map& arguments);

    BIND_METHOD()
    virtual void onCancel(const RuntimeValue::Map& arguments);

    BIND_METHOD()
    virtual void onClick(const RuntimeValue::Map& arguments);

    BIND_METHOD()
    virtual bool onMouseButtonDown(const RuntimeValue::Map& arguments);

    BIND_METHOD()
    virtual void onHover(const RuntimeValue::Map& arguments);

    BIND_METHOD()
    virtual void onUnHover(const RuntimeValue::Map& arguments);

    BIND_METHOD()
    virtual void onMouseMoved(const RuntimeValue::Map& arguments);

    BIND_METHOD()
    virtual void onMouseWheelScrolled(const RuntimeValue::Map& arguments);

    BIND_METHOD()
    virtual void onKeyDown(const RuntimeValue::Map& arguments);

    BIND_METHOD()
    virtual void onKeyUp(const RuntimeValue::Map& arguments);

    BIND_METHOD()
    virtual void onTick(float deltaTime);

    BIND_METHOD()
    virtual void onLateTick(float deltaTime);

    BIND_METHOD()
    virtual void onFixedTick(float fixedDelta);

protected:
    static FunctionalInputProvider* inputProvider();
    void resetPointerInteraction();
    virtual bool acceptsTouchCapture() const;
    virtual void onTouchCaptureBegan(const sf::Vector2f& position);
    bool hasTouchCapture() const;
    virtual void onPointerInteractionReset();
    virtual void onInteractionStateChanged();

private:
    friend class ControlBase;

    enum class PointerSource {
        None,
        Mouse,
        Touch,
    };

    static RuntimeValue::Map pointerArguments(const sf::Vector2f& position);
    static RuntimeValue::Map mouseButtonArguments(const sf::Vector2f& position,
                                                  sf::Mouse::Button button);
    static RuntimeValue::Map mouseWheelArguments(const sf::Vector2f& position,
                                                 float delta);
    void setHovered(bool hovered, const sf::Vector2f& position);
    void beginMousePress(sf::Mouse::Button button);
    void beginTouchPress();
    void endPointerPress();

    static FunctionalInputProvider* inputProvider_;
    static FocusResolver keyboardFocusResolver_;
    static DirectionalFocusRequester directionalFocusRequester_;
    static FocusSetter keyboardFocusSetter_;
    static FocusResolver keyboardCursorResolver_;

    bool hovered_ = false;
    bool pressed_ = false;
    bool active_ = true;
    PointerSource pointerSource_ = PointerSource::None;
    std::optional<sf::Mouse::Button> pressedMouseButton_;
    std::optional<sf::FloatRect> touchHitBounds_;
    EventCallback confirmCallback_;
    EventCallback cancelCallback_;
    EventCallback clickCallback_;
    HandledEventCallback mouseButtonDownCallback_;
    EventCallback hoverCallback_;
    EventCallback unHoverCallback_;
    EventCallback mouseMovedCallback_;
    EventCallback mouseWheelScrolledCallback_;
    EventCallback keyDownCallback_;
    EventCallback keyUpCallback_;
};
