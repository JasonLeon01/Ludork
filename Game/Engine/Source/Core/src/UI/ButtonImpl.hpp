#pragma once

#include <UI/Button.hpp>
#include "Interaction/GamepadKeyHintImpl.hpp"

struct Button::Impl {
    std::optional<InputNamedValue> gamepadButton;
    bool gamepadLongPress = false;
    bool defaultBackground = false;
    std::unique_ptr<ludork::engine::ui_interaction::GamepadKeyHintImpl> keyHint;
};
