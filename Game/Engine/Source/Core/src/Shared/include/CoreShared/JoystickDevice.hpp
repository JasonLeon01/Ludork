#pragma once

#include <Input/JoystickButton.hpp>
#include <cstdint>

namespace ludork::engine::joystick_device {

void synchronize();
void activity(unsigned int joystickId);
void disconnect(unsigned int joystickId);
void reset();
JoystickButton::Family family(unsigned int joystickId);
JoystickButton::Family displayFamily();
std::uint64_t presentationRevision();

}  // namespace ludork::engine::joystick_device
