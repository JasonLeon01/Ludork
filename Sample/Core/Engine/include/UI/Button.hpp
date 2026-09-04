#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <UI/FunctionalBase.hpp>
#include <UI/Image.hpp>

#include <SFML/Graphics/Color.hpp>

#include <memory>
#include <optional>

BIND_CLASS()
class Button : public Image, public FunctionalBase {
public:
    BIND_INIT()
    explicit Button(std::shared_ptr<sf::Texture> texture,
                    std::optional<sf::IntRect> rect = std::nullopt,
                    sf::Color hoverColour = sf::Color::White,
                    sf::Color pressedColour = sf::Color::White);
    virtual ~Button() = default;

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

private:
    static sf::Color multiplyColour(const sf::Color& base,
                                    const sf::Color& tint);
    void applyInteractionColour();

    sf::Color colour_ = sf::Color::White;
    sf::Color hoverColour_ = sf::Color::White;
    sf::Color pressedColour_ = sf::Color::White;
};
