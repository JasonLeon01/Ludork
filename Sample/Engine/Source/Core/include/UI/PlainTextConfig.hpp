#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <UI/TextOutlineConfig.hpp>
#include <UI/TextGlowConfig.hpp>
#include <UI/TextGradientConfig.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct LUDORK_ENGINE_API PlainTextConfig : public RuntimeObject {
    BIND_PROPERTY()
    std::string type = "plainTextConfig";

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    std::shared_ptr<sf::Font> font;

    BIND_PROPERTY()
    unsigned int characterSize = 30;

    BIND_PROPERTY()
    std::uint32_t style = sf::Text::Regular;

    BIND_PROPERTY()
    float slantAngle = 0.0f;

    BIND_PROPERTY()
    sf::Color fillColor = sf::Color::White;

    BIND_PROPERTY()
    float letterSpacing = 1.0f;

    BIND_PROPERTY()
    float lineSpacing = 1.0f;

    BIND_PROPERTY()
    sf::Text::LineAlignment lineAlignment = sf::Text::LineAlignment::Default;

    BIND_PROPERTY()
    TextOutlineConfig outline;

    BIND_PROPERTY()
    TextGlowConfig glow;

    BIND_PROPERTY()
    TextGradientConfig gradient;
};
