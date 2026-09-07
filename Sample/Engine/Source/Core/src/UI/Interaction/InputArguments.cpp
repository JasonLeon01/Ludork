#include "InputArguments.hpp"

namespace ludork::engine::ui_interaction {

std::optional<sf::Vector2f> pointerPosition(
    const UiInputEventArguments& arguments) {
    return arguments.position;
}

std::optional<sf::Mouse::Button> pointerMouseButton(
    const UiInputEventArguments& arguments) {
    return arguments.button;
}

std::optional<int> pointerButtonIndex(const UiInputEventArguments& arguments) {
    return arguments.button
               ? std::optional<int>(static_cast<int>(*arguments.button))
               : std::nullopt;
}

}  // namespace ludork::engine::ui_interaction
