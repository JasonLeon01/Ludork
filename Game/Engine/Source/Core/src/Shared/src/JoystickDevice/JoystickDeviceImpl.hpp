#pragma once

#include <Input/JoystickButton.hpp>
#include <SFML/Window/Joystick.hpp>
#include <array>
#include <cstdint>
#include <optional>

namespace ludork::engine::joystick_device {

class JoystickDeviceImpl {
public:
    static JoystickDeviceImpl& instance();
    static JoystickButton::Family classify(
        const sf::Joystick::Identification& identification);
    void synchronize();
    void activity(unsigned int joystickId);
    void disconnect(unsigned int joystickId);
    void reset();
    JoystickButton::Family family(unsigned int joystickId);
    JoystickButton::Family displayFamily();
    std::uint64_t presentationRevision() const;

private:
    struct Device {
        bool connected = false;
        sf::Joystick::Identification identification;
        JoystickButton::Family family = JoystickButton::Family::Unknown;
    };
    void refresh(unsigned int joystickId);
    void selectAvailable();
    std::array<Device, sf::Joystick::Count> devices_;
    std::optional<unsigned int> active_;
    std::uint64_t revision_ = 0;
};

}  // namespace ludork::engine::joystick_device
