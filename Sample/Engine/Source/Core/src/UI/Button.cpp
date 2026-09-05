#include <UI/Button.hpp>

#include <cstdint>
#include <utility>

Button::Button(std::shared_ptr<sf::Texture> texture,
               std::optional<sf::IntRect> rect, sf::Color hoverColour,
               sf::Color pressedColour)
    : Image(std::move(texture), rect),
      FunctionalBase(),
      hoverColour_(hoverColour),
      pressedColour_(pressedColour) {
    applyInteractionColour();
}

void Button::setVisible(bool visible) {
    ControlBase::setVisible(visible);
    if (!visible) {
        resetPointerInteraction();
    } else {
        applyInteractionColour();
    }
}

void Button::setColour(const sf::Color& colour) {
    colour_ = colour;
    applyInteractionColour();
}

sf::Color Button::getColour() const {
    return colour_;
}

void Button::setHoverColour(const sf::Color& colour) {
    hoverColour_ = colour;
    applyInteractionColour();
}

sf::Color Button::getHoverColour() const {
    return hoverColour_;
}

void Button::setPressedColour(const sf::Color& colour) {
    pressedColour_ = colour;
    applyInteractionColour();
}

sf::Color Button::getPressedColour() const {
    return pressedColour_;
}

void Button::onInteractionStateChanged() {
    applyInteractionColour();
}

sf::Color Button::multiplyColour(const sf::Color& base, const sf::Color& tint) {
    const auto multiplyChannel = [](std::uint8_t left,
                                    std::uint8_t right) -> std::uint8_t {
        const std::uint16_t product = static_cast<std::uint16_t>(left) *
                                      static_cast<std::uint16_t>(right);
        return static_cast<std::uint8_t>((product + 127u) / 255u);
    };
    return {
        multiplyChannel(base.r, tint.r),
        multiplyChannel(base.g, tint.g),
        multiplyChannel(base.b, tint.b),
        multiplyChannel(base.a, tint.a),
    };
}

void Button::applyInteractionColour() {
    sf::Color colour = colour_;
    if (getVisible() && getActive()) {
        if (isPressed()) {
            colour = multiplyColour(colour_, pressedColour_);
        } else if (isHovered()) {
            colour = multiplyColour(colour_, hoverColour_);
        }
    }
    SpriteBase::setColour(colour);
}
