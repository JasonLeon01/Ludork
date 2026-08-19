#include <Input/InputAction.hpp>

namespace {

bool lessThan(float left, float right) {
    return left < right;
}

bool greaterThan(float left, float right) {
    return left > right;
}

}  // namespace

const std::unordered_map<std::string, InputNamedValue> inputJoystickButtons = {
    {"A", {"A", static_cast<int>(JoystickButton::A)}},
    {"B", {"B", static_cast<int>(JoystickButton::B)}},
    {"X", {"X", static_cast<int>(JoystickButton::X)}},
    {"Y", {"Y", static_cast<int>(JoystickButton::Y)}},
    {"LB", {"LB", static_cast<int>(JoystickButton::LB)}},
    {"RB", {"RB", static_cast<int>(JoystickButton::RB)}},
    {"View", {"View", static_cast<int>(JoystickButton::View)}},
    {"Menu", {"Menu", static_cast<int>(JoystickButton::Menu)}},
    {"LS", {"LS", static_cast<int>(JoystickButton::LS)}},
    {"RS", {"RS", static_cast<int>(JoystickButton::RS)}},
    {"XBox", {"XBox", static_cast<int>(JoystickButton::XBox)}},
    {"Share", {"Share", static_cast<int>(JoystickButton::Share)}},
};

const std::unordered_map<std::string, InputNamedValue> inputTypes = {
    {"Mouse", {"Mouse", static_cast<int>(InputType::Mouse)}},
    {"Gamepad", {"Gamepad", static_cast<int>(InputType::Gamepad)}},
};

const std::unordered_map<std::string, int> inputActionKinds = {
    {"KeyOrScan", static_cast<int>(InputActionKind::KeyOrScan)},
    {"Key", static_cast<int>(InputActionKind::Key)},
    {"Scan", static_cast<int>(InputActionKind::Scan)},
    {"MouseButton", static_cast<int>(InputActionKind::MouseButton)},
    {"JoystickButton", static_cast<int>(InputActionKind::JoystickButton)},
    {"JoystickAxis", static_cast<int>(InputActionKind::JoystickAxis)},
    {"TouchTap", static_cast<int>(InputActionKind::TouchTap)},
};

const std::unordered_map<std::string, InputAxisComparison>
    inputAxisComparisons = {
        {"Less", lessThan},
        {"Greater", greaterThan},
};

bool InputActionKey::operator==(const InputActionKey& other) const {
    if (kind != other.kind || name != other.name || code != other.code ||
        threshold != other.threshold) {
        return false;
    }
    if (kind != InputActionKind::JoystickAxis) {
        return true;
    }
    if (comparisonIdentity || other.comparisonIdentity) {
        return comparisonIdentity && other.comparisonIdentity &&
               comparisonIdentity->equals(*other.comparisonIdentity);
    }
    if (!comparison || !other.comparison) {
        return static_cast<bool>(comparison) ==
               static_cast<bool>(other.comparison);
    }
    using FunctionPointer = bool (*)(float, float);
    const FunctionPointer* left = comparison.target<FunctionPointer>();
    const FunctionPointer* right = other.comparison.target<FunctionPointer>();
    return left != nullptr && right != nullptr && *left == *right;
}
