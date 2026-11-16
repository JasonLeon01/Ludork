#pragma once

#include <CoreMinimal.hpp>

using ContextSettingValue =
    std::variant<unsigned int, bool, std::monostate, sf::OpenGLSettings,
                 sf::OpenGLESSettings, sf::VulkanSettings, sf::DirectXSettings,
                 sf::MetalSettings>;
using ContextSettingMap = std::unordered_map<std::string, ContextSettingValue>;

BIND_CLASS(name = "ContextSettings")
class ModifiedContextSettings : public sf::ContextSettings {
public:
    BIND_INIT()
    ModifiedContextSettings(
        unsigned int depthBits = 0, unsigned int stencilBits = 0,
        unsigned int antiAliasingLevel = 0, bool sRgbCapable = false,
        const sf::ContextSettings::BackendSettings& backend = {});

    BIND_INIT()
    explicit ModifiedContextSettings(const ContextSettingMap& values);
};
