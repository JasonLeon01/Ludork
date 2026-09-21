#include "JoystickDevicePlatform.hpp"

#import <GameController/GameController.h>
#include <cstring>
#include <optional>

namespace ludork::engine::joystick_device {

JoystickButton::Family appleControllerFamily(const sf::String& name) {
    @autoreleasepool {
        std::optional<JoystickButton::Family> result;
        for (GCController* controller in [GCController controllers]) {
            GCExtendedGamepad* gamepad = controller.extendedGamepad;
            if (gamepad == nil) {
                continue;
            }
            NSString* nativeName = controller.vendorName;
            if (nativeName.length == 0) {
                nativeName = controller.productCategory;
            }
            sf::String candidate = "Game Controller";
            if (const char* utf8 = nativeName.UTF8String;
                utf8 != nullptr && *utf8 != '\0') {
                candidate =
                    sf::String::fromUtf8(utf8, utf8 + std::strlen(utf8));
            }
            if (candidate != name) {
                continue;
            }
            JoystickButton::Family family = JoystickButton::Family::Unknown;
            if (@available(iOS 14.5, *)) {
                if ([gamepad isKindOfClass:[GCDualSenseGamepad class]]) {
                    family = JoystickButton::Family::PlayStation5;
                }
            }
            if (@available(iOS 13.0, *)) {
                if ([gamepad isKindOfClass:[GCDualShockGamepad class]]) {
                    family = JoystickButton::Family::PlayStation4;
                } else if ([gamepad isKindOfClass:[GCXboxGamepad class]]) {
                    family = JoystickButton::Family::Xbox;
                }
            }
            if (result && *result != family) {
                return JoystickButton::Family::Unknown;
            }
            result = family;
        }
        return result.value_or(JoystickButton::Family::Unknown);
    }
}

}  // namespace ludork::engine::joystick_device
