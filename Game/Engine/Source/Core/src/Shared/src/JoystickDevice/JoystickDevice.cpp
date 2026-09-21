#include <CoreShared/JoystickDevice.hpp>
#include "JoystickDeviceImpl.hpp"

namespace ludork::engine::joystick_device {

void synchronize() {
    JoystickDeviceImpl::instance().synchronize();
}

void activity(unsigned int joystickId) {
    JoystickDeviceImpl::instance().activity(joystickId);
}

void disconnect(unsigned int joystickId) {
    JoystickDeviceImpl::instance().disconnect(joystickId);
}

void reset() {
    JoystickDeviceImpl::instance().reset();
}

JoystickButton::Family family(unsigned int joystickId) {
    return JoystickDeviceImpl::instance().family(joystickId);
}

JoystickButton::Family displayFamily() {
    return JoystickDeviceImpl::instance().displayFamily();
}

std::uint64_t presentationRevision() {
    return JoystickDeviceImpl::instance().presentationRevision();
}

}  // namespace ludork::engine::joystick_device
