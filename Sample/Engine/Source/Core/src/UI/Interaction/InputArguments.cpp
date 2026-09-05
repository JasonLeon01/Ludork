#include "InputArguments.hpp"

#include <cstdint>

namespace ludork::engine::ui_interaction {

std::optional<double> numericValue(RuntimeValueView value) {
    if (const double* number = value.getIf<double>()) {
        return *number;
    }
    if (const std::int64_t* number = value.getIf<std::int64_t>()) {
        return static_cast<double>(*number);
    }
    return std::nullopt;
}

std::optional<sf::Vector2f> pointerPosition(
    const RuntimeValue::Map& arguments) {
    const auto positionIterator = arguments.find("position");
    if (positionIterator == arguments.end()) {
        return std::nullopt;
    }
    std::optional<RuntimeMapView> position =
        RuntimeValueView(positionIterator->second).map();
    if (!position) {
        return std::nullopt;
    }
    const auto xIterator = position->find("x");
    const auto yIterator = position->find("y");
    if (!xIterator || !yIterator) {
        return std::nullopt;
    }
    const std::optional<double> x = numericValue(*xIterator);
    const std::optional<double> y = numericValue(*yIterator);
    if (!x.has_value() || !y.has_value()) {
        return std::nullopt;
    }
    return sf::Vector2f(static_cast<float>(*x), static_cast<float>(*y));
}

std::optional<sf::Mouse::Button> pointerMouseButton(
    const RuntimeValue::Map& arguments) {
    const auto iterator = arguments.find("button");
    if (iterator == arguments.end()) {
        return std::nullopt;
    }
    const std::int64_t* value = iterator->second.getIf<std::int64_t>();
    if (value == nullptr) {
        return std::nullopt;
    }
    return static_cast<sf::Mouse::Button>(*value);
}

std::optional<int> pointerButtonIndex(const RuntimeValue::Map& arguments) {
    const auto iterator = arguments.find("button");
    if (iterator == arguments.end()) {
        return std::nullopt;
    }
    const std::optional<double> value = numericValue(iterator->second);
    return value.has_value() ? std::optional<int>(static_cast<int>(*value))
                             : std::nullopt;
}

}  // namespace ludork::engine::ui_interaction
