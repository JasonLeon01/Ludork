#include "KeyHintImpl.hpp"

#include <Input/InputService.hpp>
#include <LudorkPlatform.hpp>

#include <SFML/Window/Keyboard.hpp>

#include <cstdint>
#include <stdexcept>

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

bool keyboardHintsAvailableWithoutJoystick() {
#if defined(SFML_SYSTEM_HARMONY)
    if (ludork::standard::harmonyDeviceType() == "tablet") {
        return true;
    }
#endif
    return !ludork::standard::isMobileDevice();
}

}  // namespace ludork::engine::tab_view_impl
