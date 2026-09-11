#pragma once

#include <Input/JoystickButton.hpp>
#include <UI/PlainText.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <memory>
#include <optional>

namespace ludork::engine::ui_interaction {

class GamepadGlyphImpl {
public:
    explicit GamepadGlyphImpl(const std::shared_ptr<PlainTextConfig>& config);
    void setButton(const InputNamedValue& button);
    void setText(const std::string& text);
    void refresh();
    void layout(const sf::Vector2f& position, const sf::Vector2f& size);
    void setColour(const sf::Color& colour);
    void refreshDisplayScale();
    void draw(sf::RenderTarget& target, const sf::RenderStates& states) const;

private:
    void rebuild();
    std::optional<InputNamedValue> button_;
    JoystickButton::Family family_ = JoystickButton::Family::Unknown;
    PlainText label_;
    sf::VertexArray vertices_{sf::PrimitiveType::Triangles};
    sf::Vector2f position_;
    sf::Vector2f size_;
    sf::Color colour_ = sf::Color::Black;
};

}  // namespace ludork::engine::ui_interaction
