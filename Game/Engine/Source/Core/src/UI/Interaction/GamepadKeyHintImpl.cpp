#include "GamepadKeyHintImpl.hpp"

#include <EngineState.hpp>
#include <Input/JoystickButton.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace ludork::engine::ui_interaction {
namespace {

constexpr float InnerInset = 2.0f;
constexpr float TrackThickness = 1.0f;
constexpr float ContentRatio = 0.8f;
constexpr std::size_t PointCount = 48;

sf::Color modulate(const sf::Color& colour, const sf::Color& presentation) {
    const auto channel = [](std::uint8_t left, std::uint8_t right) {
        return static_cast<std::uint8_t>(
            (static_cast<unsigned int>(left) * right + 127u) / 255u);
    };
    return {
        channel(colour.r, presentation.r), channel(colour.g, presentation.g),
        channel(colour.b, presentation.b), channel(colour.a, presentation.a)};
}

}  // namespace

GamepadKeyHintImpl::GamepadKeyHintImpl(
    const InputNamedValue& button, bool longPress,
    const std::shared_ptr<PlainTextConfig>& textConfig)
    : button_(button), longPress_(longPress) {
    if (!JoystickButton::isValid(button)) {
        throw std::invalid_argument(
            "Gamepad hint button must match a registered Engine.JoystickButton "
            "value");
    }
    background_.setPointCount(PointCount);
    track_.setPointCount(PointCount);
    track_.setFillColor(sf::Color::Transparent);
    setTextConfig(textConfig);
    setColour(true, sf::Color::White);
}

void GamepadKeyHintImpl::setTextConfig(
    const std::shared_ptr<PlainTextConfig>& textConfig) {
    if (textConfig == nullptr) {
        throw std::invalid_argument(
            "Gamepad hint text config must not be null");
    }
    std::shared_ptr<PlainTextConfig> config =
        std::make_shared<PlainTextConfig>(*textConfig);
    config->characterSize = 12;
    config->style = sf::Text::Regular;
    config->slantAngle = 0.0f;
    config->fillColor = sf::Color::White;
    config->outline.color = sf::Color::Transparent;
    config->outline.thickness = 0.0f;
    config->glow = {};
    config->gradient = {};
    label_ = std::make_unique<PlainText>(config, button_.name);
    layout(position_);
}

void GamepadKeyHintImpl::setLongPress(bool longPress) {
    longPress_ = longPress;
    triggered_ = false;
    resetProgress();
    layout(position_);
}

bool GamepadKeyHintImpl::updateHold(bool down, bool enabled, float deltaTime) {
    if (!down) {
        triggered_ = false;
    }
    if (!down || !enabled || !longPress_) {
        resetProgress();
        return false;
    }
    if (triggered_) {
        return false;
    }
    const float elapsed =
        std::isfinite(deltaTime) ? std::max(0.0f, deltaTime) : 0.0f;
    holdTime_ = std::min(holdTime_ + elapsed, LongPressDuration);
    progress_.setAngle(sf::degrees(holdTime_ / LongPressDuration * 360.0f));
    triggered_ = holdTime_ >= LongPressDuration;
    return triggered_;
}

void GamepadKeyHintImpl::resetProgress() {
    holdTime_ = 0.0f;
    progress_.setAngle(sf::degrees(0.0f));
}

void GamepadKeyHintImpl::layout(const sf::Vector2f& position) {
    position_ = position;
    const float scale = engineState().getScale();
    const float radius = Diameter * 0.5f;
    const float innerRadius = longPress_ ? radius - InnerInset : radius;
    track_.setRadius(radius * scale);
    track_.setOutlineThickness(-TrackThickness * scale);
    track_.setPosition(position * scale);
    progress_.setRadius(radius * scale);
    progress_.setPosition(position * scale);
    background_.setRadius(innerRadius * scale);
    background_.setPosition(
        (position + sf::Vector2f(radius - innerRadius, radius - innerRadius)) *
        scale);
    const sf::FloatRect bounds = label_->getLocalBounds();
    const float content = innerRadius * 2.0f * ContentRatio;
    float labelScale = 1.0f;
    if (bounds.size.x > 0.0f) {
        labelScale = std::min(labelScale, content / bounds.size.x);
    }
    if (bounds.size.y > 0.0f) {
        labelScale = std::min(labelScale, content / bounds.size.y);
    }
    label_->setScale({labelScale, labelScale});
    label_->setPosition(position + sf::Vector2f(radius, radius) -
                        (bounds.position + bounds.size * 0.5f) * labelScale);
}

void GamepadKeyHintImpl::setColour(bool enabled,
                                   const sf::Color& presentation) {
    const sf::Color background =
        enabled ? sf::Color::White : sf::Color(150, 150, 150, 200);
    background_.setFillColor(modulate(background, presentation));
    track_.setOutlineColor(modulate(background, presentation));
    progress_.setFillColour(modulate(sf::Color(0, 255, 0), presentation));
    label_->setColour(modulate(sf::Color::Black, presentation));
}

void GamepadKeyHintImpl::refreshDisplayScale() {
    label_->refreshDisplayScale();
    layout(position_);
}

void GamepadKeyHintImpl::draw(sf::RenderTarget& target,
                              const sf::RenderStates& states) const {
    if (longPress_) {
        target.draw(track_, states);
        target.draw(progress_, states);
    }
    target.draw(background_, states);
    target.draw(*label_, states);
}

}  // namespace ludork::engine::ui_interaction
