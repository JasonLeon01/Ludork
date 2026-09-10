#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>

#include <optional>

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
