#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct TextStyle {
    BIND_PROPERTY()
    std::optional<unsigned int> characterSize;

    BIND_PROPERTY()
    std::optional<bool> bold;

    BIND_PROPERTY()
    std::optional<bool> italic;

    BIND_PROPERTY()
    std::optional<bool> underlined;

    BIND_PROPERTY()
    std::optional<bool> strikeThrough;

    BIND_PROPERTY()
    std::optional<sf::Color> fillColor;

    BIND_PROPERTY()
    std::optional<float> letterSpacing;

    BIND_PROPERTY()
    std::optional<float> lineSpacing;

    BIND_PROPERTY()
    std::optional<sf::Color> outlineColor;

    BIND_PROPERTY()
    std::optional<float> outlineThickness;

    BIND_METHOD()
    void enableStyle(sf::Text& text) const;

    BIND_METHOD()
    void adaptStyle(const TextStyle& inStyle);

    BIND_METHOD(name = "_copy", metadata = false)
    TextStyle copy() const;
};
