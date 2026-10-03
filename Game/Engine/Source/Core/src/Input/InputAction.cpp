#include <Input/InputAction.hpp>
#include <Input/InputNamedValue.hpp>
#include <Input/JoystickButton.hpp>

namespace {

bool lessThan(float left, float right) {
    return left < right;
}

bool greaterThan(float left, float right) {
    return left > right;
}

}  // namespace

const std::unordered_map<std::string, InputNamedValue> inputTypes = {
    {"Mouse", {"Mouse", static_cast<int>(InputType::Mouse)}},
    {"Gamepad", {"Gamepad", static_cast<int>(InputType::Gamepad)}},
};

const std::unordered_map<std::string, InputAxisComparison>
    inputAxisComparisons = {
        {"Less", lessThan},
        {"Greater", greaterThan},
};
