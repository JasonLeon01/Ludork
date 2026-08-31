#pragma once

#include <BindAnnotations.hpp>

#include <SFML/Window/ContextSettings.hpp>

#include <string>
#include <unordered_map>
#include <variant>

using ContextSettingValue = std::variant<unsigned int, bool>;
using ContextSettingMap = std::unordered_map<std::string, ContextSettingValue>;

BIND_CLASS(name = "ContextSettings")
class ModifiedContextSettings : public sf::ContextSettings {
public:
    BIND_INIT()
    ModifiedContextSettings(unsigned int depthBits = 0,
                            unsigned int stencilBits = 0,
                            unsigned int antiAliasingLevel = 0,
                            unsigned int majorVersion = 1,
                            unsigned int minorVersion = 1,
                            unsigned int attributeFlags = 0,
                            bool sRgbCapable = false);

    BIND_INIT()
    explicit ModifiedContextSettings(const ContextSettingMap& values);
};
