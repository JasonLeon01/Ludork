#include <Light.hpp>

namespace {

float getFloat(const LightValue &value, float fallback) {
    if (const auto floatValue = std::get_if<float>(&value)) {
        return *floatValue;
    }
    if (const auto intValue = std::get_if<int>(&value)) {
        return static_cast<float>(*intValue);
    }
    return fallback;
}

std::vector<float> getFloatList(const LightValue &value) {
    if (const auto floatValues = std::get_if<std::vector<float>>(&value)) {
        return *floatValues;
    }
    if (const auto intValues = std::get_if<std::vector<int>>(&value)) {
        std::vector<float> result;
        result.reserve(intValues->size());
        for (int item : *intValues) {
            result.push_back(static_cast<float>(item));
        }
        return result;
    }
    return {};
}

int clampColor(float value) {
    if (value < 0.0f) {
        return 0;
    }
    if (value > 255.0f) {
        return 255;
    }
    return static_cast<int>(value);
}

}  // namespace

Light::Light(const sf::Vector2f &position, const sf::Color &colour, float radius, float intensity)
    : position(position), colour(colour), radius(radius), intensity(intensity) {}

Light::Light(const std::pair<float, float> &position, const sf::Color &colour, float radius, float intensity)
    : Light(sf::Vector2f(position.first, position.second), colour, radius, intensity) {}

LightData Light::asDict() const {
    return {
        {"position", std::vector<float>{position.x, position.y}},
        {"color",
         std::vector<int>{static_cast<int>(colour.r), static_cast<int>(colour.g), static_cast<int>(colour.b),
                          static_cast<int>(colour.a)}},
        {"radius", radius},
        {"intensity", intensity},
    };
}

Light Light::fromDict(const LightData &data) {
    Light light;

    auto positionIt = data.find("position");
    if (positionIt != data.end()) {
        auto positionValues = getFloatList(positionIt->second);
        if (positionValues.size() >= 2) {
            light.position = sf::Vector2f(positionValues[0], positionValues[1]);
        }
    }

    auto colorIt = data.find("color");
    if (colorIt != data.end()) {
        auto colorValues = getFloatList(colorIt->second);
        while (colorValues.size() < 4) {
            colorValues.push_back(255.0f);
        }
        light.colour = sf::Color(clampColor(colorValues[0]), clampColor(colorValues[1]), clampColor(colorValues[2]),
                                 clampColor(colorValues[3]));
    }

    auto radiusIt = data.find("radius");
    if (radiusIt != data.end()) {
        light.radius = getFloat(radiusIt->second, light.radius);
    }

    auto intensityIt = data.find("intensity");
    if (intensityIt != data.end()) {
        light.intensity = getFloat(intensityIt->second, light.intensity);
    }

    return light;
}
