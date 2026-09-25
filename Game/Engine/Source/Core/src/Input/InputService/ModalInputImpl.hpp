#pragma once

#include <Input/InputCapture.hpp>
#include <Input/InjectedInputEvent.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace ludork::engine::input_impl {

class ModalInputImpl {
public:
    bool captured() const;
    std::shared_ptr<InputCapture> capture();
    void setConfirmEnabled(InputCapture& capture, bool enabled);
    bool consumeConfirm(InputCapture& capture, bool focused);
    bool beginFrame();
    void finishFrame();
    void observeInjectedEvent(const InjectedInputEvent& event,
                              sf::Keyboard::Key key,
                              sf::Keyboard::Scancode scan,
                              sf::Mouse::Button button,
                              bool acceptsConfirmation);
    bool observeNativeEvent(const sf::Event& event, bool acceptsKeyboardMouse,
                            bool acceptsJoystick, bool acceptsPointer);
    void observeNativeTouch(const sf::Event& event,
                            const sf::Vector2i& position,
                            bool acceptsConfirmation, float dragThreshold);
    void suppressKey(sf::Keyboard::Key key);
    bool allowsJoystickButton(unsigned int joystickId,
                              unsigned int button) const;
    bool allowsMouseButton(sf::Mouse::Button button) const;
    void reset();

private:
    std::shared_ptr<InputCapture> activeCapture() const;
    void confirm();
    void clearConfirmations();
    void clearNativeControls();
    void cancelTouchConfirmation();
    void updateTouchTravel(const sf::Vector2i& position, float dragThreshold);
    void synchronizeHeldConfirmControls();
    void observeNativeConfirmation(const sf::Event& event,
                                   bool acceptsKeyboardMouse,
                                   bool acceptsJoystick, bool acceptsPointer);
    bool physicalInputHeld() const;
    void synchronizeSuppressedControls(bool capture);

    std::uint64_t revision_ = 0;
    bool waitingForRelease_ = false;
    bool frameCaptured_ = false;
    bool suppressNextText_ = false;
    std::vector<std::weak_ptr<InputCapture>> captures_;
    std::unordered_set<std::string> injectedKeys_;
    std::unordered_set<sf::Mouse::Button> injectedButtons_;
    std::unordered_set<sf::Keyboard::Key> nativeKeys_;
    std::unordered_set<sf::Keyboard::Scancode> nativeScans_;
    std::unordered_set<sf::Mouse::Button> nativeMouseButtons_;
    std::unordered_map<unsigned int, std::unordered_set<unsigned int>>
        nativeJoystickButtons_;
    std::unordered_set<unsigned int> nativeTouches_;
    std::weak_ptr<InputCapture> touchCapture_;
    std::optional<unsigned int> touchFinger_;
    sf::Vector2i touchPosition_;
    float touchTravelDistance_ = 0.0f;
    std::unordered_set<sf::Keyboard::Key> suppressedKeys_;
    std::unordered_set<sf::Keyboard::Scancode> suppressedScans_;
    std::unordered_set<sf::Mouse::Button> suppressedMouseButtons_;
    std::unordered_map<unsigned int, std::unordered_set<unsigned int>>
        suppressedJoystickButtons_;
    std::unordered_map<unsigned int, std::unordered_set<sf::Joystick::Axis>>
        suppressedJoystickAxes_;
};

}  // namespace ludork::engine::input_impl
