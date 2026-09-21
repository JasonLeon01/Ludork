#include "JoystickDeviceImpl.hpp"
#include "JoystickDevicePlatform.hpp"

#include <SFML/Config.hpp>
#include <algorithm>
#include <cctype>
#include <string>

namespace ludork::engine::joystick_device {

JoystickDeviceImpl& JoystickDeviceImpl::instance() {
    static JoystickDeviceImpl impl;
    return impl;
}

JoystickButton::Family JoystickDeviceImpl::classify(
    const sf::Joystick::Identification& identification) {
    const bool sony = identification.vendorId == 0x054c;
#if defined(SFML_SYSTEM_HARMONY)
    const bool sonyProduct = sony || identification.vendorId == 0;
#else
    const bool sonyProduct = sony;
#endif
    if (sonyProduct) {
        switch (identification.productId) {
            case 0x05c4:
            case 0x09cc:
            case 0x0ba0:
                return JoystickButton::Family::PlayStation4;
            case 0x0ce6:
            case 0x0df2:
                return JoystickButton::Family::PlayStation5;
        }
    }
    if (identification.vendorId == 0x045e) {
        return JoystickButton::Family::Xbox;
    }
#if defined(SFML_SYSTEM_IOS)
    return appleControllerFamily(identification.name);
#elif defined(SFML_SYSTEM_HARMONY)
    std::string name = identification.name.toAnsiString();
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    if (name.find("dualsense") != std::string::npos) {
        return JoystickButton::Family::PlayStation5;
    }
    if (name.find("dualshock 4") != std::string::npos ||
        name.find("dualshock4") != std::string::npos) {
        return JoystickButton::Family::PlayStation4;
    }
    if (name.find("xbox") != std::string::npos) {
        return JoystickButton::Family::Xbox;
    }
#endif
    return JoystickButton::Family::Unknown;
}

void JoystickDeviceImpl::refresh(unsigned int joystickId) {
    if (joystickId >= devices_.size()) {
        return;
    }
    if (!sf::Joystick::isConnected(joystickId)) {
        disconnect(joystickId);
        return;
    }
    Device& device = devices_[joystickId];
    const sf::Joystick::Identification identification =
        sf::Joystick::getIdentification(joystickId);
    if (!device.connected ||
        device.identification.name != identification.name ||
        device.identification.vendorId != identification.vendorId ||
        device.identification.productId != identification.productId) {
        device.connected = true;
        device.identification = identification;
        device.family = classify(identification);
        ++revision_;
    }
}

void JoystickDeviceImpl::selectAvailable() {
    if (active_ && devices_[*active_].connected) {
        return;
    }
    active_.reset();
    for (unsigned int index = 0; index < devices_.size(); ++index) {
        if (devices_[index].connected) {
            active_ = index;
            break;
        }
    }
}

void JoystickDeviceImpl::synchronize() {
#if defined(SFML_SYSTEM_IOS)
    const std::uint64_t revision = revision_;
#endif
    for (unsigned int index = 0; index < devices_.size(); ++index) {
        refresh(index);
    }
#if defined(SFML_SYSTEM_IOS)
    if (revision != revision_) {
        for (Device& device : devices_) {
            if (!device.connected) {
                continue;
            }
            const JoystickButton::Family family =
                classify(device.identification);
            if (family != device.family) {
                device.family = family;
                ++revision_;
            }
        }
    }
#endif
    selectAvailable();
}

void JoystickDeviceImpl::activity(unsigned int joystickId) {
    refresh(joystickId);
    if (joystickId < devices_.size() && devices_[joystickId].connected &&
        active_ != joystickId) {
        active_ = joystickId;
        ++revision_;
    }
}

void JoystickDeviceImpl::disconnect(unsigned int joystickId) {
    if (joystickId >= devices_.size() || !devices_[joystickId].connected) {
        return;
    }
    devices_[joystickId] = {};
    ++revision_;
    selectAvailable();
}

void JoystickDeviceImpl::reset() {
    devices_ = {};
    active_.reset();
    ++revision_;
}

JoystickButton::Family JoystickDeviceImpl::family(unsigned int joystickId) {
    refresh(joystickId);
    return joystickId < devices_.size() ? devices_[joystickId].family
                                        : JoystickButton::Family::Unknown;
}

JoystickButton::Family JoystickDeviceImpl::displayFamily() {
    selectAvailable();
    return active_ ? devices_[*active_].family
                   : JoystickButton::Family::Unknown;
}

std::uint64_t JoystickDeviceImpl::presentationRevision() const {
    return revision_;
}

}  // namespace ludork::engine::joystick_device
