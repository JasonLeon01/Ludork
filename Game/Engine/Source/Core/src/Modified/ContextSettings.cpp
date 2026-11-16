#include <Modified/ContextSettings.hpp>

#include <stdexcept>
#include <type_traits>

ModifiedContextSettings::ModifiedContextSettings(
    unsigned int depthBits, unsigned int stencilBits,
    unsigned int antiAliasingLevel, bool sRgbCapable,
    const sf::ContextSettings::BackendSettings& backend)
    : sf::ContextSettings{} {
    this->depthBits = depthBits;
    this->stencilBits = stencilBits;
    this->antiAliasingLevel = antiAliasingLevel;
    this->sRgbCapable = sRgbCapable;
    this->backend = backend;
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
        } else if (field == "backend") {
            std::visit(
                [this](const auto& settings) {
                    if constexpr (std::is_constructible_v<
                                      sf::ContextSettings::BackendSettings,
                                      decltype(settings)>) {
                        backend = settings;
                    } else {
                        throw std::invalid_argument(
                            "ContextSettings.backend requires backend settings "
                            "or nil");
                    }
                },
                value);
        } else if (field == "sRgbCapable") {
            sRgbCapable = std::get<bool>(value);
        } else {
            throw std::invalid_argument("Unknown ContextSettings field: " +
                                        field);
        }
    }
}
