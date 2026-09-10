#include <Modified/ContextSettings.hpp>

#include <stdexcept>

ModifiedContextSettings::ModifiedContextSettings(
    unsigned int depthBits, unsigned int stencilBits,
    unsigned int antiAliasingLevel, unsigned int majorVersion,
    unsigned int minorVersion, unsigned int attributeFlags, bool sRgbCapable)
    : sf::ContextSettings{} {
    this->depthBits = depthBits;
    this->stencilBits = stencilBits;
    this->antiAliasingLevel = antiAliasingLevel;
    this->majorVersion = majorVersion;
    this->minorVersion = minorVersion;
    this->attributeFlags = attributeFlags;
    this->sRgbCapable = sRgbCapable;
}

ModifiedContextSettings::ModifiedContextSettings(
    const ContextSettingMap& values) {
    for (const auto& [field, value] : values) {
        if (field == "depthBits") {
            depthBits = std::get<unsigned int>(value);
        } else if (field == "stencilBits") {
            stencilBits = std::get<unsigned int>(value);
        } else if (field == "antiAliasingLevel") {
            antiAliasingLevel = std::get<unsigned int>(value);
        } else if (field == "majorVersion") {
            majorVersion = std::get<unsigned int>(value);
        } else if (field == "minorVersion") {
            minorVersion = std::get<unsigned int>(value);
        } else if (field == "attributeFlags") {
            attributeFlags = static_cast<sf::ContextSettings::Attribute>(
                std::get<unsigned int>(value));
        } else if (field == "sRgbCapable") {
            sRgbCapable = std::get<bool>(value);
        } else {
            throw std::invalid_argument("Unknown ContextSettings field: " +
                                        field);
        }
    }
}
