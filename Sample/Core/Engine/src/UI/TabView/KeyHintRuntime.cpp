#include "KeyHintRuntime.hpp"

#include <Input/InputService.hpp>
#include <LudorkCoreBinding/RegistryReference.hpp>
#include <LudorkPlatform.hpp>

#include <SFML/Window/Joystick.hpp>
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

std::string keyboardKeyText(const RuntimeValue& value,
                            const std::string& source) {
    const std::int64_t* code = value.getIf<std::int64_t>();
    if (code == nullptr || *code < 0 ||
        static_cast<std::uint64_t>(*code) >= sf::Keyboard::KeyCount) {
        throw std::invalid_argument(
            source + " must be a valid non-Unknown sf.Keyboard.Key");
    }
    const sf::Keyboard::Key key = static_cast<sf::Keyboard::Key>(*code);
    const sf::Keyboard::Scancode scan = sf::Keyboard::delocalize(key);
    std::string result = toUtf8String(sf::Keyboard::getDescription(scan));
    return result.empty() ? std::to_string(*code) : result;
}

std::string handleKeyText(const RuntimeValue& value,
                          const std::string& source) {
    const RuntimeIdentityPtr* identity = value.getIf<RuntimeIdentityPtr>();
    const auto* opaque =
        identity == nullptr || *identity == nullptr
            ? nullptr
            : dynamic_cast<
                  const ludork_core::LuaOpaqueIdentity<RuntimeIdentity>*>(
                  identity->get());
    if (opaque == nullptr) {
        throw std::invalid_argument(source +
                                    " must be an Engine.JoystickButton value");
    }
    sol::state_view lua(opaque->value().state());
    const sol::object luaValue =
        ludork_core::readLuaRegistryReference(lua, opaque->value());
    if (!luaValue.is<sol::table>()) {
        throw std::invalid_argument(source +
                                    " must be an Engine.JoystickButton value");
    }
    const sol::object engineValue =
        lua.globals().raw_get<sol::object>("Engine");
    if (!engineValue.is<sol::table>()) {
        throw std::invalid_argument(source +
                                    " must be an Engine.JoystickButton value");
    }
    const sol::object inputValue =
        engineValue.as<sol::table>().raw_get<sol::object>("Input");
    if (!inputValue.is<sol::table>()) {
        throw std::invalid_argument(source +
                                    " must be an Engine.JoystickButton value");
    }
    const sol::object reverseValue =
        inputValue.as<sol::table>().raw_get<sol::object>("JoyStickButtonName");
    if (!reverseValue.is<sol::table>()) {
        throw std::invalid_argument(source +
                                    " must be an Engine.JoystickButton value");
    }
    const sol::object nameValue =
        reverseValue.as<sol::table>().raw_get<sol::object>(luaValue);
    if (!nameValue.is<std::string>()) {
        throw std::invalid_argument(source +
                                    " must be an Engine.JoystickButton value");
    }
    const std::string name = nameValue.as<std::string>();
    const auto iterator = inputJoystickButtons.find(name);
    if (iterator == inputJoystickButtons.end()) {
        throw std::invalid_argument(
            source + " must match a registered Engine.JoystickButton value");
    }
    return iterator->second.name;
}

}  // namespace

KeyHint parseKeyHint(const RuntimeValue::Map& values,
                     const std::string& source) {
    KeyHint result;
    for (const auto& [name, value] : values) {
        if (name == "Keyboard") {
            result.keyboard = keyboardKeyText(value, source + ".Keyboard");
        } else if (name == "Joystick") {
            result.handle = handleKeyText(value, source + ".Handle");
        } else {
            throw std::invalid_argument(source + " has unknown key " + name);
        }
    }
    return result;
}

bool anyJoystickConnected() {
    for (unsigned int joystickId = 0; joystickId < sf::Joystick::Count;
         ++joystickId) {
        if (sf::Joystick::isConnected(joystickId)) {
            return true;
        }
    }
    return false;
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
