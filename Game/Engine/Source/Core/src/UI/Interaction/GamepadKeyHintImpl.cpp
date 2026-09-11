#include "GamepadKeyHintImpl.hpp"

#include <EngineState.hpp>
#include <Input/JoystickButton.hpp>
#include <Input/InputService.hpp>

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
    glyph_ = std::make_unique<GamepadGlyphImpl>(config);
    glyph_->setButton(button_);
    layout(position_);
}

void GamepadKeyHintImpl::setLongPress(bool longPress) {
    longPress_ = longPress;
    consumed_ = {};
    resetProgress();
    layout(position_);
}

void GamepadKeyHintImpl::refresh() {
    glyph_->refresh();
    const std::uint64_t revision = JoystickButton::getPresentationRevision();
    if (presentationRevision_ != revision) {
        presentationRevision_ = revision;
        heldJoystick_.reset();
        resetProgress();
    }
}

bool GamepadKeyHintImpl::updateHold(bool enabled, float deltaTime) {
    refresh();
    std::array<bool, sf::Joystick::Count> down{};
    for (unsigned int joystickId = 0; joystickId < down.size(); ++joystickId) {
        down[joystickId] =
            inputService().isJoystickButtonValueDown(joystickId, button_);
        if (!down[joystickId]) {
            consumed_[joystickId] = false;
        }
    }
    if (!enabled || !longPress_) {
        heldJoystick_.reset();
        resetProgress();
        return false;
    }
    if (heldJoystick_ && (!down[*heldJoystick_] || consumed_[*heldJoystick_])) {
        heldJoystick_.reset();
        resetProgress();
    }
    if (!heldJoystick_) {
        for (unsigned int joystickId = 0; joystickId < down.size();
             ++joystickId) {
            if (down[joystickId] && !consumed_[joystickId]) {
                heldJoystick_ = joystickId;
                break;
            }
        }
    }
    if (!heldJoystick_) {
        return false;
    }
    const float elapsed =
        std::isfinite(deltaTime) ? std::max(0.0f, deltaTime) : 0.0f;
    holdTime_ = std::min(holdTime_ + elapsed, LongPressDuration);
    progress_.setAngle(sf::degrees(holdTime_ / LongPressDuration * 360.0f));
    if (holdTime_ >= LongPressDuration) {
        consumed_[*heldJoystick_] = true;
        return true;
    }
    return false;
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
    const float content = innerRadius * 2.0f * ContentRatio;
    const sf::Vector2f extent{content, content};
    glyph_->layout(position + sf::Vector2f(radius, radius) - extent * 0.5f,
                   extent);
}

void GamepadKeyHintImpl::setColour(bool enabled,
                                   const sf::Color& presentation) {
    const sf::Color background =
        enabled ? sf::Color::White : sf::Color(150, 150, 150, 200);
    background_.setFillColor(modulate(background, presentation));
    track_.setOutlineColor(modulate(background, presentation));
    progress_.setFillColour(modulate(sf::Color(0, 255, 0), presentation));
    glyph_->setColour(modulate(sf::Color::Black, presentation));
}

void GamepadKeyHintImpl::refreshDisplayScale() {
    glyph_->refreshDisplayScale();
    layout(position_);
}

void GamepadKeyHintImpl::draw(sf::RenderTarget& target,
                              const sf::RenderStates& states) const {
    if (longPress_) {
        target.draw(track_, states);
        target.draw(progress_, states);
    }
    target.draw(background_, states);
    glyph_->draw(target, states);
}

}  // namespace ludork::engine::ui_interaction
