#pragma once

#include <Input/InjectedInputEvent.hpp>
#include <SFML/Window/Event.hpp>

#include <cstdint>
#include <string>
#include <unordered_set>
#include <unordered_map>

namespace ludork::engine::input_impl {

class ModalInputImpl {
public:
    bool captured() const;
    bool beginFrame();
    void finishFrame();
    void observeInjectedEvent(const InjectedInputEvent& event);
    bool observeNativeEvent(const sf::Event& event);
    void suppressKey(sf::Keyboard::Key key);
    bool allowsJoystickButton(unsigned int joystickId,
                              unsigned int button) const;
    bool allowsMouseButton(sf::Mouse::Button button) const;
    void reset();

private:
    bool physicalInputHeld() const;
    void synchronizeSuppressedControls(bool capture);

    std::uint64_t revision_ = 0;
    bool waitingForRelease_ = false;
    bool frameCaptured_ = false;
    bool suppressNextText_ = false;
    std::unordered_set<std::string> injectedKeys_;
    std::unordered_set<std::string> injectedButtons_;
    std::unordered_set<sf::Keyboard::Key> suppressedKeys_;
    std::unordered_set<sf::Keyboard::Scancode> suppressedScans_;
    std::unordered_set<sf::Mouse::Button> suppressedMouseButtons_;
    std::unordered_map<unsigned int, std::unordered_set<unsigned int>>
        suppressedJoystickButtons_;
    std::unordered_map<unsigned int, std::unordered_set<sf::Joystick::Axis>>
        suppressedJoystickAxes_;
};

}  // namespace ludork::engine::input_impl
