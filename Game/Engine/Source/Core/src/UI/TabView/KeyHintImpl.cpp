#include "KeyHintImpl.hpp"
#include <Input/InputNamedValue.hpp>
#include <Input/JoystickButton.hpp>

#include <Input/InputService.hpp>
#include <LudorkPlatform.hpp>

#include <SFML/Window/Keyboard.hpp>

#if defined(SFML_SYSTEM_HARMONY) && defined(SFML_HARMONY_MOBILE)
#include <deviceinfo.h>
#endif

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace ludork::engine::tab_view_impl {
namespace {

std::string toUtf8String(const sf::String& value) {
    const sf::U8String bytes = value.toUtf8();
    return {bytes.begin(), bytes.end()};
}

}  // namespace

std::string keyboardKeyText(sf::Keyboard::Key key, const std::string& source) {
    const int code = static_cast<int>(key);
    if (code < 0 || static_cast<unsigned int>(code) >= sf::Keyboard::KeyCount) {
        throw std::invalid_argument(
            source + " must be a valid non-Unknown sf.Keyboard.Key");
    }
    const sf::Keyboard::Scancode scan = sf::Keyboard::delocalize(key);
    std::string result = toUtf8String(sf::Keyboard::getDescription(scan));
    return result.empty() ? std::to_string(code) : result;
}

std::string handleKeyText(const InputNamedValue& button,
                          const std::string& source) {
    if (!JoystickButton::isValid(button)) {
        throw std::invalid_argument(
            source + " must match a registered Engine.JoystickButton value");
    }
    return button.name;
}

bool keyboardHintsAvailableWithoutJoystick() {
#if !defined(LUDORK_MOBILE)
    return true;
#elif defined(SFML_SYSTEM_HARMONY) && defined(SFML_HARMONY_MOBILE)
    const char* deviceType = OH_GetDeviceType();
    return deviceType != nullptr && std::string_view(deviceType) == "tablet";
#else
    return false;
#endif
}

}  // namespace ludork::engine::tab_view_impl
