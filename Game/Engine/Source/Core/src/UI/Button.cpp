#include <UI/Button.hpp>
#include "ButtonImpl.hpp"
#include "Interaction/JoystickState.hpp"
#include "Text/TextConfigCodec.hpp"

#include <Input/InputService.hpp>
#include <UI/PlainTextConfig.hpp>

#include <cstdint>
#include <utility>

Button::Button(std::shared_ptr<sf::Texture> texture,
               std::optional<sf::IntRect> rect, sf::Color hoverColour,
               sf::Color pressedColour)
    : Image(std::move(texture), rect),
      FunctionalBase(),
      hoverColour_(hoverColour),
      pressedColour_(pressedColour),
      impl_(std::make_unique<Impl>()) {
    applyInteractionColour();
}

Button::~Button() = default;

void Button::setTexture(std::shared_ptr<sf::Texture> texture, bool resetRect) {
    SpriteBase::setTexture(std::move(texture), resetRect);
    impl_->defaultBackground = false;
}

void Button::setDefaultBackgroundTexture(std::shared_ptr<sf::Texture> texture,
                                         bool resetRect) {
    SpriteBase::setTexture(std::move(texture), resetRect);
    impl_->defaultBackground = true;
}

void Button::setGamepadButton(const std::optional<InputNamedValue>& button) {
    if (button.has_value()) {
        std::shared_ptr<PlainTextConfig> config =
            std::make_shared<PlainTextConfig>();
        config->font =
            ludork::engine::text_config::loadFont("", "Button gamepad hint");
        std::unique_ptr<ludork::engine::ui_interaction::GamepadKeyHintImpl>
            hint = std::make_unique<
                ludork::engine::ui_interaction::GamepadKeyHintImpl>(
                *button, impl_->gamepadLongPress, config);
        impl_->keyHint = std::move(hint);
    } else {
        impl_->keyHint.reset();
    }
    impl_->gamepadButton = button;
}

std::optional<InputNamedValue> Button::getGamepadButton() const {
    return impl_->gamepadButton;
}

void Button::setGamepadLongPress(bool longPress) {
    if (impl_->gamepadLongPress == longPress) {
        return;
    }
    impl_->gamepadLongPress = longPress;
    if (impl_->keyHint != nullptr) {
        impl_->keyHint->setLongPress(longPress);
    }
}

bool Button::getGamepadLongPress() const {
    return impl_->gamepadLongPress;
}

void Button::update(float deltaTime) {
    FunctionalBase::update(deltaTime);
    if (impl_->keyHint == nullptr) {
        return;
    }
    const bool enabled = isInteractionEnabled() && inputService().isFocused() &&
                         ludork::engine::ui_interaction::anyJoystickConnected();
    impl_->keyHint->refresh();
    bool triggered = false;
    if (impl_->gamepadLongPress) {
        triggered = impl_->keyHint->updateHold(enabled, deltaTime);
    } else if (enabled) {
        triggered = inputService().isAnyJoystickButtonValueTriggered(
            *impl_->gamepadButton, true);
    }
    if (triggered) {
        onConfirm({});
    }
}

void Button::refreshDisplayScale() {
    if (impl_->keyHint != nullptr) {
        impl_->keyHint->refreshDisplayScale();
    }
    ControlBase::refreshDisplayScale();
}

void Button::releaseRuntimeCallbacks() noexcept {
    onInteractionInvalidated();
    ControlBase::releaseRuntimeCallbacks();
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

void Button::onInteractionInvalidated() {
    if (impl_->keyHint != nullptr) {
        impl_->keyHint->resetProgress();
    }
}

void Button::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (!getVisible()) {
        return;
    }
    const bool gamepad = ludork::engine::ui_interaction::anyJoystickConnected();
    if (!gamepad || !impl_->defaultBackground) {
        Image::draw(target, states);
    }
    const sf::Vector2f scale = getScale();
    if (!gamepad || impl_->keyHint == nullptr || scale.x <= 0.0f ||
        scale.y <= 0.0f) {
        return;
    }
    constexpr float diameter =
        ludork::engine::ui_interaction::GamepadKeyHintImpl::Diameter;
    const sf::Vector2f size = getSize().componentWiseMul(scale);
    impl_->keyHint->layout({size.x - diameter, (size.y - diameter) * 0.5f});
    impl_->keyHint->setColour(
        isInteractionEnabled(),
        multiplyColour(getColour(), presentationColour()));
    ControlBase::_applyRenderStates(states);
    states.transform.scale({1.0f / scale.x, 1.0f / scale.y});
    impl_->keyHint->draw(target, states);
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
