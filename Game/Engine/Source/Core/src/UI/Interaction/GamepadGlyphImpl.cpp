#include "GamepadGlyphImpl.hpp"

#include <EngineState.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace ludork::engine::ui_interaction {
namespace {

void line(sf::VertexArray& vertices, sf::Vector2f from, sf::Vector2f to) {
    const sf::Vector2f delta = to - from;
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    const sf::Vector2f normal{-delta.y / length * 0.055f,
                              delta.x / length * 0.055f};
    for (const sf::Vector2f point : {from + normal, from - normal, to + normal,
                                     to + normal, from - normal, to - normal}) {
        vertices.append(sf::Vertex{point, sf::Color::Black});
    }
}

std::string buttonText(const std::string& name, JoystickButton::Family family) {
    if (family != JoystickButton::Family::PlayStation4 &&
        family != JoystickButton::Family::PlayStation5) {
        return name;
    }
    if (name == "LB") {
        return "L1";
    }
    if (name == "RB") {
        return "R1";
    }
    if (name == "LS") {
        return "L3";
    }
    if (name == "RS") {
        return "R3";
    }
    if (name == "Menu") {
        return "Options";
    }
    if (name == "View") {
        return family == JoystickButton::Family::PlayStation4 ? "Share"
                                                              : "Create";
    }
    if (name == "XBox") {
        return "PS";
    }
    if (name == "Share") {
        return "";
    }
    return name;
}

}  // namespace

GamepadGlyphImpl::GamepadGlyphImpl(
    const std::shared_ptr<PlainTextConfig>& config)
    : label_(config, "") {
    label_.setColour(colour_);
}

void GamepadGlyphImpl::setButton(const InputNamedValue& button) {
    if (!JoystickButton::isValid(button)) {
        throw std::invalid_argument(
            "Gamepad glyph requires a registered JoystickButton value");
    }
    const JoystickButton::Family family = JoystickButton::getDisplayFamily();
    if (button_ && button_->name == button.name &&
        button_->value == button.value && family_ == family) {
        return;
    }
    button_ = button;
    family_ = family;
    rebuild();
}

void GamepadGlyphImpl::setText(const std::string& text) {
    if (!button_ && label_.getString() == text) {
        return;
    }
    button_.reset();
    vertices_.clear();
    label_.setString(text);
    layout(position_, size_);
}

void GamepadGlyphImpl::refresh() {
    if (button_ && family_ != JoystickButton::getDisplayFamily()) {
        family_ = JoystickButton::getDisplayFamily();
        rebuild();
    }
}

void GamepadGlyphImpl::rebuild() {
    vertices_.clear();
    const std::string& name = button_->name;
    const bool playStation = family_ == JoystickButton::Family::PlayStation4 ||
                             family_ == JoystickButton::Family::PlayStation5;
    if (playStation && name == "A") {
        line(vertices_, {0.18f, 0.18f}, {0.82f, 0.82f});
        line(vertices_, {0.82f, 0.18f}, {0.18f, 0.82f});
    } else if (playStation && name == "B") {
        for (unsigned int index = 0; index < 64; ++index) {
            const float angle = static_cast<float>(index) * 2.0f *
                                std::numbers::pi_v<float> / 64.0f;
            const float next = static_cast<float>(index + 1) * 2.0f *
                               std::numbers::pi_v<float> / 64.0f;
            const sf::Vector2f centre{0.5f, 0.5f};
            const sf::Vector2f startDirection{std::cos(angle), std::sin(angle)};
            const sf::Vector2f endDirection{std::cos(next), std::sin(next)};
            const sf::Vector2f outerStart = centre + startDirection * 0.395f;
            const sf::Vector2f innerStart = centre + startDirection * 0.285f;
            const sf::Vector2f outerEnd = centre + endDirection * 0.395f;
            const sf::Vector2f innerEnd = centre + endDirection * 0.285f;
            for (const sf::Vector2f point : {outerStart, innerStart, outerEnd,
                                             outerEnd, innerStart, innerEnd}) {
                vertices_.append(sf::Vertex{point, sf::Color::Black});
            }
        }
    } else if (playStation && name == "X") {
        line(vertices_, {0.18f, 0.18f}, {0.82f, 0.18f});
        line(vertices_, {0.82f, 0.18f}, {0.82f, 0.82f});
        line(vertices_, {0.82f, 0.82f}, {0.18f, 0.82f});
        line(vertices_, {0.18f, 0.82f}, {0.18f, 0.18f});
    } else if (playStation && name == "Y") {
        line(vertices_, {0.5f, 0.13f}, {0.88f, 0.8f});
        line(vertices_, {0.88f, 0.8f}, {0.12f, 0.8f});
        line(vertices_, {0.12f, 0.8f}, {0.5f, 0.13f});
    }
    label_.setString(vertices_.getVertexCount() == 0 ? buttonText(name, family_)
                                                     : "");
    setColour(colour_);
    layout(position_, size_);
}

void GamepadGlyphImpl::layout(const sf::Vector2f& position,
                              const sf::Vector2f& size) {
    position_ = position;
    size_ = size;
    const sf::FloatRect bounds = label_.getLocalBounds();
    float scale = 1.0f;
    if (bounds.size.x > 0.0f) {
        scale = std::min(scale, size.x / bounds.size.x);
    }
    if (bounds.size.y > 0.0f) {
        scale = std::min(scale, size.y / bounds.size.y);
    }
    label_.setScale({scale, scale});
    label_.setPosition(position + size * 0.5f -
                       (bounds.position + bounds.size * 0.5f) * scale);
}

void GamepadGlyphImpl::setColour(const sf::Color& colour) {
    colour_ = colour;
    label_.setColour(colour);
    for (std::size_t index = 0; index < vertices_.getVertexCount(); ++index) {
        vertices_[index].color = colour;
    }
}

void GamepadGlyphImpl::refreshDisplayScale() {
    label_.refreshDisplayScale();
    layout(position_, size_);
}

void GamepadGlyphImpl::draw(sf::RenderTarget& target,
                            const sf::RenderStates& states) const {
    if (vertices_.getVertexCount() != 0) {
        sf::RenderStates glyphStates = states;
        glyphStates.texture = nullptr;
        const float scale = engineState().getScale();
        glyphStates.transform.translate(position_ * scale);
        glyphStates.transform.scale(size_ * scale);
        target.draw(vertices_, glyphStates);
    } else {
        target.draw(label_, states);
    }
}

}  // namespace ludork::engine::ui_interaction
