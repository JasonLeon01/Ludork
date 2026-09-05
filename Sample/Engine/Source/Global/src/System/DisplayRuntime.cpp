#include "DisplayRuntime.hpp"

#include <algorithm>
#include <cmath>

namespace ludork::global::system_display_impl {

bool viewsEqual(const sf::View& left, const sf::View& right) {
    return left.getCenter() == right.getCenter() &&
           left.getSize() == right.getSize() &&
           left.getRotation() == right.getRotation() &&
           left.getViewport() == right.getViewport() &&
           left.getScissor() == right.getScissor();
}

float windowFitScale(const sf::Vector2u& surfaceSize,
                     const sf::Vector2u& gameSize) {
    const float scale = std::min(
        static_cast<float>(surfaceSize.x) / static_cast<float>(gameSize.x),
        static_cast<float>(surfaceSize.y) / static_cast<float>(gameSize.y));
    return std::max(0.01f, scale);
}

float effectiveRenderScale(float surfaceFitScale, float maximumRenderScale) {
    const float normalizedSurfaceFitScale = std::max(0.01f, surfaceFitScale);
    const float effectiveScale =
        maximumRenderScale > 0.0f
            ? std::min(normalizedSurfaceFitScale, maximumRenderScale)
            : normalizedSurfaceFitScale;
    return std::max(0.01f, effectiveScale);
}

sf::Vector2u scaledSize(const sf::Vector2u& gameSize, float scale) {
    const float normalizedScale = std::max(0.01f, scale);
    return {
        static_cast<unsigned int>(std::max(
            1.0f,
            std::floor(static_cast<float>(gameSize.x) * normalizedScale))),
        static_cast<unsigned int>(std::max(
            1.0f,
            std::floor(static_cast<float>(gameSize.y) * normalizedScale))),
    };
}

}  // namespace ludork::global::system_display_impl
