#include "ScreenEffectsRuntime.hpp"

#include <algorithm>
#include <cmath>

namespace ludork::global::system_screen_effects_impl {

sf::Glsl::Vec4 makeToneColour(float red, float green, float blue, float gray) {
    return {std::clamp(red, -255.0f, 255.0f) / 255.0f,
            std::clamp(green, -255.0f, 255.0f) / 255.0f,
            std::clamp(blue, -255.0f, 255.0f) / 255.0f,
            std::clamp(gray, 0.0f, 255.0f) / 255.0f};
}

sf::Glsl::Vec4 interpolateTone(const sf::Glsl::Vec4& start,
                               const sf::Glsl::Vec4& target, float ratio) {
    const float amount = std::clamp(ratio, 0.0f, 1.0f);
    return {start.x + (target.x - start.x) * amount,
            start.y + (target.y - start.y) * amount,
            start.z + (target.z - start.z) * amount,
            start.w + (target.w - start.w) * amount};
}

bool isNeutralTone(const sf::Glsl::Vec4& colour) {
    return std::abs(colour.x) <= 0.0001f && std::abs(colour.y) <= 0.0001f &&
           std::abs(colour.z) <= 0.0001f && std::abs(colour.w) <= 0.0001f;
}

}  // namespace ludork::global::system_screen_effects_impl
