#include "TextCommon.hpp"

#include <Runtime/EngineState.hpp>

#include <cstdint>

void TextStyle::enableStyle(sf::Text& text) const {
    if (characterSize.has_value()) {
        text.setCharacterSize(
            ludork::engine::text_detail::scaledCharacterSize(*characterSize));
    }
    std::uint32_t resolvedStyle = text.getStyle();
    const auto applyStyleFlag = [&resolvedStyle](
                                    const std::optional<bool>& enabled,
                                    sf::Text::Style style) {
        if (!enabled.has_value()) {
            return;
        }
        const std::uint32_t mask = static_cast<std::uint32_t>(style);
        if (*enabled) {
            resolvedStyle |= mask;
        } else {
            resolvedStyle &= ~mask;
        }
    };
    applyStyleFlag(bold, sf::Text::Bold);
    applyStyleFlag(italic, sf::Text::Italic);
    applyStyleFlag(underlined, sf::Text::Underlined);
    applyStyleFlag(strikeThrough, sf::Text::StrikeThrough);
    text.setStyle(resolvedStyle);
    if (fillColor.has_value()) {
        text.setFillColor(*fillColor);
    }
    if (letterSpacing.has_value()) {
        text.setLetterSpacing(*letterSpacing);
    }
    if (lineSpacing.has_value()) {
        text.setLineSpacing(*lineSpacing);
    }
    if (outlineColor.has_value()) {
        text.setOutlineColor(*outlineColor);
    }
    if (outlineThickness.has_value()) {
        text.setOutlineThickness(*outlineThickness * Scale);
    }
}

void TextStyle::adaptStyle(const TextStyle& inStyle) {
    if (inStyle.characterSize.has_value()) {
        characterSize = inStyle.characterSize;
    }
    if (inStyle.bold.has_value()) {
        bold = inStyle.bold;
    }
    if (inStyle.italic.has_value()) {
        italic = inStyle.italic;
    }
    if (inStyle.underlined.has_value()) {
        underlined = inStyle.underlined;
    }
    if (inStyle.strikeThrough.has_value()) {
        strikeThrough = inStyle.strikeThrough;
    }
    if (inStyle.fillColor.has_value()) {
        fillColor = inStyle.fillColor;
    }
    if (inStyle.letterSpacing.has_value()) {
        letterSpacing = inStyle.letterSpacing;
    }
    if (inStyle.lineSpacing.has_value()) {
        lineSpacing = inStyle.lineSpacing;
    }
    if (inStyle.outlineColor.has_value()) {
        outlineColor = inStyle.outlineColor;
    }
    if (inStyle.outlineThickness.has_value()) {
        outlineThickness = inStyle.outlineThickness;
    }
}

TextStyle TextStyle::copy() const {
    return *this;
}
