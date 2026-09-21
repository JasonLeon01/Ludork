#include <Input/JoystickButton.hpp>
#include <Input/InputNamedValue.hpp>
#include <CoreShared/JoystickDevice.hpp>

#include <SFML/Config.hpp>

#include <array>
#include <string_view>
#include <stdexcept>
#include <utility>

namespace {

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

constexpr std::array<ButtonEntry, 11> PlayStationDesktopButtons = {
    ButtonEntry{"A", 1},    ButtonEntry{"B", 2},     ButtonEntry{"X", 0},
    ButtonEntry{"Y", 3},    ButtonEntry{"LB", 4},    ButtonEntry{"RB", 5},
    ButtonEntry{"View", 8}, ButtonEntry{"Menu", 9},  ButtonEntry{"LS", 10},
    ButtonEntry{"RS", 11},  ButtonEntry{"XBox", 12},
};

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

std::optional<InputNamedValue> JoystickButton::fromName(
    const std::string& name) {
    if (name.empty()) {
        return std::nullopt;
    }
    for (const auto& [buttonName, value] : PlatformButtons) {
        if (buttonName == name) {
            return get(name);
        }
    }
    throw std::invalid_argument("Unknown gamepad button: " + name);
}

std::optional<unsigned int> JoystickButton::resolve(
    unsigned int joystickId, const InputNamedValue& button) {
    if (joystickId >= sf::Joystick::Count ||
        !sf::Joystick::isConnected(joystickId)) {
        return std::nullopt;
    }
    int raw = button.value;
    if (!button.name.empty()) {
        if (!isValid(button)) {
            return std::nullopt;
        }
        const Family family = getFamily(joystickId);
        if (family == Family::PlayStation4 || family == Family::PlayStation5) {
            if (button.name == "Share") {
                return std::nullopt;
            }
#if defined(SFML_SYSTEM_WINDOWS) || defined(SFML_SYSTEM_MACOS)
            for (const auto& [name, value] : PlayStationDesktopButtons) {
                if (name == button.name) {
                    raw = value;
                    break;
                }
            }
#endif
        }
    }
    if (raw < 0 || static_cast<unsigned int>(raw) >=
                       sf::Joystick::getButtonCount(joystickId)) {
        return std::nullopt;
    }
    return static_cast<unsigned int>(raw);
}

JoystickButton::Family JoystickButton::getFamily(unsigned int joystickId) {
    return ludork::engine::joystick_device::family(joystickId);
}

JoystickButton::Family JoystickButton::getDisplayFamily() {
    return ludork::engine::joystick_device::displayFamily();
}

std::uint64_t JoystickButton::getPresentationRevision() {
    return ludork::engine::joystick_device::presentationRevision();
}
