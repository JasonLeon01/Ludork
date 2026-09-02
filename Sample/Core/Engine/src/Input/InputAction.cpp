#include <Input/InputAction.hpp>

#include <SFML/Config.hpp>

#include <array>
#include <string_view>
#include <utility>

namespace {

bool lessThan(float left, float right) {
    return left < right;
}

bool greaterThan(float left, float right) {
    return left > right;
}

using ButtonEntry = std::pair<std::string_view, int>;

#if defined(SFML_SYSTEM_MACOS)

constexpr std::array<ButtonEntry, 12> PlatformButtons = {
    ButtonEntry{"A", 0},     ButtonEntry{"B", 1},     ButtonEntry{"X", 3},
    ButtonEntry{"Y", 4},     ButtonEntry{"LB", 6},    ButtonEntry{"RB", 7},
    ButtonEntry{"View", 10}, ButtonEntry{"Menu", 11}, ButtonEntry{"LS", 13},
    ButtonEntry{"RS", 14},   ButtonEntry{"XBox", 12}, ButtonEntry{"Share", 15},
};

#elif defined(SFML_SYSTEM_IOS) || defined(SFML_SYSTEM_ANDROID) || \
    defined(SFML_SYSTEM_HARMONY)

constexpr std::array<ButtonEntry, 12> PlatformButtons = {
    ButtonEntry{"A", 0},     ButtonEntry{"B", 1},     ButtonEntry{"X", 3},
    ButtonEntry{"Y", 4},     ButtonEntry{"LB", 6},    ButtonEntry{"RB", 7},
    ButtonEntry{"View", 13}, ButtonEntry{"Menu", 12}, ButtonEntry{"LS", 10},
    ButtonEntry{"RS", 11},   ButtonEntry{"XBox", 14}, ButtonEntry{"Share", 15},
};

#else

constexpr std::array<ButtonEntry, 12> PlatformButtons = {
    ButtonEntry{"A", 0},    ButtonEntry{"B", 1},     ButtonEntry{"X", 2},
    ButtonEntry{"Y", 3},    ButtonEntry{"LB", 4},    ButtonEntry{"RB", 5},
    ButtonEntry{"View", 6}, ButtonEntry{"Menu", 7},  ButtonEntry{"LS", 8},
    ButtonEntry{"RS", 9},   ButtonEntry{"XBox", 10}, ButtonEntry{"Share", 11},
};

#endif

}  // namespace

std::optional<InputNamedValue> JoystickButton::get(const std::string& name) {
#if defined(SFML_SYSTEM_ANDROID) || defined(SFML_SYSTEM_HARMONY)
    if (name == "Share") {
        return std::nullopt;
    }
#endif
    for (const auto& [buttonName, value] : PlatformButtons) {
        if (buttonName == name) {
            return InputNamedValue{std::string(buttonName), value};
        }
    }
    return std::nullopt;
}

InputNamedValue JoystickButton::getA() {
    return *get("A");
}

InputNamedValue JoystickButton::getB() {
    return *get("B");
}

InputNamedValue JoystickButton::getX() {
    return *get("X");
}

InputNamedValue JoystickButton::getY() {
    return *get("Y");
}

InputNamedValue JoystickButton::getLB() {
    return *get("LB");
}

InputNamedValue JoystickButton::getRB() {
    return *get("RB");
}

InputNamedValue JoystickButton::getView() {
    return *get("View");
}

InputNamedValue JoystickButton::getMenu() {
    return *get("Menu");
}

InputNamedValue JoystickButton::getLS() {
    return *get("LS");
}

InputNamedValue JoystickButton::getRS() {
    return *get("RS");
}

InputNamedValue JoystickButton::getXBox() {
    return *get("XBox");
}

std::optional<InputNamedValue> JoystickButton::getShare() {
    return get("Share");
}

bool JoystickButton::isValid(const InputNamedValue& button) {
    const std::optional<InputNamedValue> expected = get(button.name);
    return expected.has_value() && expected->value == button.value;
}

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
