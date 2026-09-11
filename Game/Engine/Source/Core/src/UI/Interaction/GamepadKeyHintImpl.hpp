#pragma once

#include <Input/InputNamedValue.hpp>
#include "GamepadGlyphImpl.hpp"
#include <array>
#include <optional>
#include <UI/PlainTextConfig.hpp>
#include <UI/SectorShape.hpp>
#include <SFML/Window/Joystick.hpp>

namespace ludork::engine::ui_interaction {

class GamepadKeyHintImpl {
public:
    static constexpr float Diameter = 16.0f;
    static constexpr float LongPressDuration = 1.0f;

    GamepadKeyHintImpl(const InputNamedValue& button, bool longPress,
                       const std::shared_ptr<PlainTextConfig>& textConfig);

    void setTextConfig(const std::shared_ptr<PlainTextConfig>& textConfig);
    void setLongPress(bool longPress);
    void refresh();
    bool updateHold(bool enabled, float deltaTime);
    void resetProgress();
    void layout(const sf::Vector2f& position);
    void setColour(bool enabled, const sf::Color& presentation);
    void refreshDisplayScale();
    void draw(sf::RenderTarget& target, const sf::RenderStates& states) const;

private:
    InputNamedValue button_;
    bool longPress_;
    float holdTime_ = 0.0f;
    std::optional<unsigned int> heldJoystick_;
    std::array<bool, sf::Joystick::Count> consumed_{};
    std::uint64_t presentationRevision_ = 0;
    sf::Vector2f position_;
    std::unique_ptr<GamepadGlyphImpl> glyph_;
    sf::CircleShape background_;
    sf::CircleShape track_;
    SectorShape progress_;
};

}  // namespace ludork::engine::ui_interaction
