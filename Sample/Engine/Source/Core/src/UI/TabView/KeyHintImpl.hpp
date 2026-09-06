#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <optional>
#include <string>

namespace ludork::engine::tab_view_impl {

struct KeyHint {
    std::optional<std::string> keyboard;
    std::optional<std::string> handle;
};

KeyHint parseKeyHint(const RuntimeValue::Map& values,
                     const std::string& source);
bool anyJoystickConnected();
bool keyboardHintsAvailableWithoutJoystick();

}  // namespace ludork::engine::tab_view_impl
