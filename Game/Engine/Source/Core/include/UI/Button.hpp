#pragma once

#include <CoreMinimal.hpp>

#include <UI/FunctionalBase.hpp>
#include <UI/Image.hpp>
#include <Input/InputNamedValue.hpp>

BIND_CLASS()
class Button : public Image, public FunctionalBase {
public:
    BIND_INIT()
    explicit Button(std::shared_ptr<sf::Texture> texture,
                    std::optional<sf::IntRect> rect = std::nullopt,
                    sf::Color hoverColour = sf::Color::White,
                    sf::Color pressedColour = sf::Color::White);
    virtual ~Button();

    BIND_METHOD()
    void setTexture(std::shared_ptr<sf::Texture> texture,
                    bool resetRect = false);

    void setDefaultBackgroundTexture(std::shared_ptr<sf::Texture> texture,
                                     bool resetRect = false);

    BIND_METHOD()
    void setGamepadButton(const std::optional<InputNamedValue>& button);

    BIND_METHOD(Pure = true)
    std::optional<InputNamedValue> getGamepadButton() const;

    BIND_METHOD()
    void setGamepadLongPress(bool longPress);

    BIND_METHOD(Pure = true)
    bool getGamepadLongPress() const;

    BIND_METHOD()
    void update(float deltaTime) override;

    void refreshDisplayScale() override;
    void releaseRuntimeCallbacks() noexcept override;

    BIND_METHOD()
    void setVisible(bool visible) override;

    BIND_METHOD()
    void setColour(const sf::Color& colour);

    BIND_METHOD(Pure = true)
    sf::Color getColour() const;

    BIND_METHOD()
    void setHoverColour(const sf::Color& colour);

    BIND_METHOD(Pure = true)
    sf::Color getHoverColour() const;

    BIND_METHOD()
    void setPressedColour(const sf::Color& colour);

    BIND_METHOD(Pure = true)
    sf::Color getPressedColour() const;

protected:
    void onInteractionStateChanged() override;
    void onInteractionInvalidated() override;

    BIND_METHOD()
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    static sf::Color multiplyColour(const sf::Color& base,
                                    const sf::Color& tint);
    void applyInteractionColour();

    sf::Color colour_ = sf::Color::White;
    sf::Color hoverColour_ = sf::Color::White;
    sf::Color pressedColour_ = sf::Color::White;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
