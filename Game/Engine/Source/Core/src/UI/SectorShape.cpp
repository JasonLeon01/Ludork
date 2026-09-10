#include <UI/SectorShape.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr float FullTurnDegrees = 360.0f;
constexpr float SegmentDegrees = 4.0f;

}  // namespace

SectorShape::SectorShape(float radius, const sf::Color& fillColour)
    : radius_(normalizedRadius(radius)), fillColour_(fillColour) {
    rebuild();
}

float SectorShape::getRadius() const {
    return radius_;
}

void SectorShape::setRadius(float radius) {
    const float normalized = normalizedRadius(radius);
    if (radius_ == normalized) {
        return;
    }
    radius_ = normalized;
    rebuild();
}

sf::Angle SectorShape::getAngle() const {
    return angle_;
}

void SectorShape::setAngle(sf::Angle angle) {
    const sf::Angle normalized = normalizedAngle(angle);
    if (angle_ == normalized) {
        return;
    }
    angle_ = normalized;
    rebuild();
}

sf::Color SectorShape::getFillColour() const {
    return fillColour_;
}

void SectorShape::setFillColour(const sf::Color& colour) {
    if (fillColour_ == colour) {
        return;
    }
    fillColour_ = colour;
    for (std::size_t index = 0; index < vertices_.getVertexCount(); ++index) {
        vertices_[index].color = fillColour_;
    }
}

sf::FloatRect SectorShape::getLocalBounds() const {
    return {{0.0f, 0.0f}, {radius_ * 2.0f, radius_ * 2.0f}};
}

sf::FloatRect SectorShape::getGlobalBounds() const {
    return getTransform().transformRect(getLocalBounds());
}

void SectorShape::draw(sf::RenderTarget& target,
                       sf::RenderStates states) const {
    if (vertices_.getVertexCount() < 3) {
        return;
    }
    states.transform *= getTransform();
    states.texture = nullptr;
    target.draw(vertices_, states);
}

float SectorShape::normalizedRadius(float radius) {
    return std::isfinite(radius) ? std::max(0.0f, radius) : 0.0f;
}

sf::Angle SectorShape::normalizedAngle(sf::Angle angle) {
    const float degrees = angle.asDegrees();
    if (!std::isfinite(degrees)) {
        return sf::degrees(0.0f);
    }
    return sf::degrees(std::clamp(degrees, 0.0f, FullTurnDegrees));
}

void SectorShape::rebuild() {
    const float degrees = angle_.asDegrees();
    if (radius_ <= 0.0f || degrees <= 0.0f) {
        vertices_.clear();
        return;
    }
    const std::size_t segments = static_cast<std::size_t>(
        std::max(1.0f, std::ceil(degrees / SegmentDegrees)));
    vertices_.resize(segments + 2);
    const sf::Vector2f centre(radius_, radius_);
    vertices_[0] = sf::Vertex{centre, fillColour_, {}};
    const float step = degrees / static_cast<float>(segments);
    for (std::size_t index = 0; index <= segments; ++index) {
        const float radians = (step * static_cast<float>(index)) *
                              std::numbers::pi_v<float> / 180.0f;
        const sf::Vector2f offset(std::sin(radians) * radius_,
                                  -std::cos(radians) * radius_);
        vertices_[index + 1] = sf::Vertex{centre + offset, fillColour_, {}};
    }
}
