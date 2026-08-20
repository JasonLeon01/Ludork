#include "TextCommon.hpp"

#include <Runtime/EngineState.hpp>
#include <UI/TextEffects.hpp>
#include <UI/UIState.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {

int hexDigit(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

std::optional<std::uint8_t> hexByte(const std::string& value,
                                    std::size_t offset) {
    const int high = hexDigit(value[offset]);
    const int low = hexDigit(value[offset + 1]);
    if (high < 0 || low < 0) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>((high << 4) | low);
}

sf::Text::LineAlignment resolvedLineAlignment(
    sf::Text::LineAlignment alignment) {
    return alignment == sf::Text::LineAlignment::Default
               ? sf::Text::LineAlignment::Left
               : alignment;
}

float alignmentOffset(sf::Text::LineAlignment alignment, float availableWidth,
                      float lineWidth) {
    switch (resolvedLineAlignment(alignment)) {
        case sf::Text::LineAlignment::Center:
            return (availableWidth - lineWidth) * 0.5f;
        case sf::Text::LineAlignment::Right:
            return availableWidth - lineWidth;
        default:
            return 0.0f;
    }
}

}  // namespace

struct RichText::EffectCache {
    ludork::engine::text_effects::Cache data;
};

RichText::RichText(std::shared_ptr<RichTextConfig> config,
                   const std::string& text)
    : config_(snapshotConfig(config)),
      localBounds_({0.0f, 0.0f}, {0.0f, 0.0f}),
      effects_(std::make_unique<EffectCache>()),
      displayScale_(Scale) {
    setString(text);
}

RichText::~RichText() = default;

std::shared_ptr<RichTextConfig> RichText::getConfig() const {
    return snapshotConfig(*config_);
}

void RichText::setString(const std::string& text) {
    string_ = text;
    renderText(text);
    refreshSegmentColours();
    invalidateEffects();
}

const std::string& RichText::getString() const {
    return string_;
}

void RichText::setColour(const sf::Color& colour) {
    colour_ = colour;
    refreshSegmentColours();
}

sf::Color RichText::getColour() const {
    return colour_;
}

sf::FloatRect RichText::getPixelBounds() const {
    syncDisplayScale();
    return ludork::engine::text_effects::expandedBounds(localBounds_,
                                                        config_->glow);
}

sf::FloatRect RichText::getLocalBounds() const {
    const sf::FloatRect bounds = getPixelBounds();
    return {bounds.position / Scale, bounds.size / Scale};
}

sf::FloatRect RichText::getGlobalBounds() const {
    return getTransform().transformRect(getLocalBounds());
}

sf::Vector2f RichText::getSize() const {
    return getPixelBounds().size / Scale;
}

sf::Vector2f RichText::getOrigin() const {
    syncDisplayScale();
    return ControlBase::getOrigin() / Scale;
}

void RichText::setOrigin(const sf::Vector2f& origin) {
    syncDisplayScale();
    ControlBase::setOrigin(origin * Scale);
}

void RichText::refreshDisplayScale() {
    if (displayScale_ != Scale) {
        const sf::Vector2f logicalOrigin =
            ControlBase::getOrigin() / displayScale_;
        displayScale_ = Scale;
        renderText(string_);
        refreshSegmentColours();
        invalidateEffects();
        setOrigin(logicalOrigin);
    }
    ControlBase::refreshDisplayScale();
}

void RichText::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    syncDisplayScale();
    _applyRenderStates(states);
    if (!getVisible()) {
        return;
    }
    if (!ludork::engine::text_effects::enabled(config_->glow,
                                               config_->gradient)) {
        for (const Segment& segment : segments_) {
            target.draw(*segment.text, states);
        }
        return;
    }
    ensureEffects();
    if (ludork::engine::text_effects::draw(effects_->data, target, states,
                                           colour_, config_->glow,
                                           config_->gradient)) {
        return;
    }
    for (const Segment& segment : segments_) {
        target.draw(*segment.text, states);
    }
}

const RichTextConfig& RichText::configReference(
    const std::shared_ptr<RichTextConfig>& config) {
    if (config == nullptr) {
        throw std::invalid_argument("RichText config must not be null");
    }
    if (config->type != "richTextConfig") {
        throw std::invalid_argument("RichText requires a richTextConfig asset");
    }
    if (config->font == nullptr) {
        throw std::invalid_argument(
            "RichTextConfig resolved font must not be null");
    }
    ludork::engine::text_detail::validateGradient(config->gradient);
    return *config;
}

std::shared_ptr<RichTextConfig> RichText::snapshotConfig(
    const std::shared_ptr<RichTextConfig>& config) {
    return snapshotConfig(configReference(config));
}

std::shared_ptr<RichTextConfig> RichText::snapshotConfig(
    const RichTextConfig& config) {
    std::shared_ptr<RichTextConfig> snapshot =
        std::make_shared<RichTextConfig>(config);
    if (config.defaultStyle != nullptr) {
        snapshot->defaultStyle =
            std::make_shared<TextStyle>(*config.defaultStyle);
    }
    snapshot->styles.clear();
    snapshot->styles.reserve(config.styles.size());
    for (const auto& [name, style] : config.styles) {
        std::shared_ptr<TextStyle> styleSnapshot;
        if (style != nullptr) {
            styleSnapshot = std::make_shared<TextStyle>(*style);
        }
        snapshot->styles.emplace(name, std::move(styleSnapshot));
    }
    if (config.gradient.curve != nullptr) {
        snapshot->gradient.curve =
            Vector4Curve::fromData(config.gradient.curve->toData());
    }
    return snapshot;
}

std::optional<sf::Color> RichText::parseMarkerColour(
    const std::string& marker) {
    std::string value = marker;
    if (!value.empty() && (value.front() == '#' || value.front() == '$')) {
        value.erase(value.begin());
    } else if (value.size() >= 2 && value[0] == '0' && value[1] == 'x') {
        value.erase(0, 2);
    }
    if (value.size() != 6 && value.size() != 8) {
        return std::nullopt;
    }
    const std::optional<std::uint8_t> red = hexByte(value, 0);
    const std::optional<std::uint8_t> green = hexByte(value, 2);
    const std::optional<std::uint8_t> blue = hexByte(value, 4);
    if (!red.has_value() || !green.has_value() || !blue.has_value()) {
        return std::nullopt;
    }
    std::uint8_t alpha = 255;
    if (value.size() == 8) {
        const std::optional<std::uint8_t> parsedAlpha = hexByte(value, 6);
        if (!parsedAlpha.has_value()) {
            return std::nullopt;
        }
        alpha = *parsedAlpha;
    }
    return sf::Color(*red, *green, *blue, alpha);
}

sf::Color RichText::modulateColour(const sf::Color& baseColour,
                                   const sf::Color& factorColour) {
    return ludork::engine::text_detail::modulateColour(baseColour,
                                                       factorColour);
}

void RichText::renderText(const std::string& text) {
    struct PendingSegment {
        std::unique_ptr<sf::Text> text;
        TextStyle style;
        sf::FloatRect bounds;
        float baseline = 0.0f;
        float advance = 0.0f;
    };

    struct PendingLine {
        std::vector<PendingSegment> segments;
        float width = 0.0f;
        float baseline = 0.0f;
        float advance = 0.0f;
    };

    TextStyle style = createDefaultStyle();
    segments_.clear();

    std::string normalized = text;
    if (!normalized.empty() && normalized.back() == '\n') {
        normalized.pop_back();
    }
    normalized.push_back('\n');

    std::vector<PendingLine> lines;
    float maximumLineWidth = 0.0f;
    std::size_t lineStart = 0;
    while (lineStart < normalized.size()) {
        const std::size_t lineEnd = normalized.find('\n', lineStart);
        const std::string line =
            normalized.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 1;

        std::vector<std::pair<std::string, TextStyle>> lineSegments;
        std::string bufferedText;
        std::string savedMarker;
        bool readingMarker = false;
        for (const char character : line) {
            if (character != '#') {
                (readingMarker ? savedMarker : bufferedText)
                    .push_back(character);
                continue;
            }
            if (readingMarker) {
                readingMarker = false;
                const std::optional<TextStyle> targetStyle =
                    resolveStyleMarker(savedMarker);
                if (!targetStyle.has_value()) {
                    bufferedText += "#" + savedMarker + "#";
                } else {
                    if (!bufferedText.empty()) {
                        lineSegments.emplace_back(bufferedText, style.copy());
                        bufferedText.clear();
                    }
                    style.adaptStyle(*targetStyle);
                }
                savedMarker.clear();
                continue;
            }
            if (!bufferedText.empty()) {
                lineSegments.emplace_back(bufferedText, style.copy());
                bufferedText.clear();
            }
            readingMarker = true;
        }
        if (readingMarker) {
            bufferedText += "#" + savedMarker;
        }
        if (!bufferedText.empty()) {
            lineSegments.emplace_back(bufferedText, style.copy());
        }

        PendingLine pendingLine;
        pendingLine.advance = getLineAdvance(style);
        for (const auto& [segmentText, segmentStyle] : lineSegments) {
            std::unique_ptr<sf::Text> textObject =
                buildText(segmentText, segmentStyle);
            const sf::FloatRect bounds = textObject->getLocalBounds();
            const float baseline = getBaseline(*textObject);
            const float advance = measureAdvance(*textObject);
            pendingLine.width += advance;
            pendingLine.baseline = std::max(pendingLine.baseline, baseline);
            pendingLine.advance =
                std::max(pendingLine.advance, getLineAdvance(segmentStyle));
            pendingLine.segments.push_back({std::move(textObject), segmentStyle,
                                            bounds, baseline, advance});
        }
        maximumLineWidth = std::max(maximumLineWidth, pendingLine.width);
        lines.push_back(std::move(pendingLine));
    }

    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float y = 0.0f;
    bool hasVisibleSegment = false;
    for (PendingLine& line : lines) {
        float x = alignmentOffset(config_->lineAlignment, maximumLineWidth,
                                  line.width);
        for (PendingSegment& pending : line.segments) {
            const float segmentY = y + line.baseline - pending.baseline;
            pending.text->setPosition({x, segmentY});
            hasVisibleSegment = true;
            minX = std::min(minX, x + pending.bounds.position.x);
            minY = std::min(minY, segmentY + pending.bounds.position.y);
            maxX = std::max(
                maxX, x + pending.bounds.position.x + pending.bounds.size.x);
            maxY = std::max(maxY, segmentY + pending.bounds.position.y +
                                      pending.bounds.size.y);
            x += pending.advance;
            segments_.push_back(
                {std::move(pending.text), std::move(pending.style)});
        }
        y += line.advance;
    }

    localBounds_ = hasVisibleSegment
                       ? sf::FloatRect({minX, minY}, {maxX - minX, maxY - minY})
                       : sf::FloatRect({0.0f, 0.0f}, {0.0f, 0.0f});
}

TextStyle RichText::createDefaultStyle() const {
    TextStyle style;
    style.characterSize =
        static_cast<unsigned int>(std::max(1, defaultFontSize));
    style.bold = false;
    style.italic = false;
    style.underlined = false;
    style.strikeThrough = false;
    style.fillColor = sf::Color::White;
    style.letterSpacing = 1.0f;
    style.lineSpacing = 1.0f;
    style.outlineColor = sf::Color::Black;
    style.outlineThickness = 0.0f;
    if (config_->defaultStyle != nullptr) {
        style.adaptStyle(*config_->defaultStyle);
    }
    return style;
}

std::optional<TextStyle> RichText::resolveStyleMarker(
    const std::string& marker) const {
    if (marker == "default") {
        return createDefaultStyle();
    }
    const auto iterator = config_->styles.find(marker);
    if (iterator != config_->styles.end() && iterator->second != nullptr) {
        return *iterator->second;
    }
    const std::optional<sf::Color> colour = parseMarkerColour(marker);
    if (!colour.has_value()) {
        return std::nullopt;
    }
    TextStyle style;
    style.fillColor = colour;
    return style;
}

std::unique_ptr<sf::Text> RichText::buildText(const std::string& text,
                                              const TextStyle& style) const {
    if (!style.characterSize.has_value()) {
        throw std::logic_error("RichText style requires a character size");
    }
    std::unique_ptr<sf::Text> result = std::make_unique<sf::Text>(
        *config_->font, ludork::engine::text_detail::toSfString(text),
        ludork::engine::text_detail::scaledCharacterSize(*style.characterSize));
    style.enableStyle(*result);
    return result;
}

float RichText::getBaseline(const sf::Text& text) {
    const std::vector<sf::Text::ShapedGlyph>& glyphs = text.getShapedGlyphs();
    if (!glyphs.empty()) {
        float result = glyphs.front().position.y;
        for (const sf::Text::ShapedGlyph& glyph : glyphs) {
            result = std::max(result, glyph.position.y);
        }
        return result;
    }
    const sf::FloatRect bounds = text.getLocalBounds();
    return bounds.position.y + bounds.size.y;
}

float RichText::measureAdvance(const sf::Text& text) {
    const std::vector<sf::Text::ShapedGlyph>& glyphs = text.getShapedGlyphs();
    if (!glyphs.empty()) {
        float result = glyphs.front().position.x + glyphs.front().glyph.advance;
        for (const sf::Text::ShapedGlyph& glyph : glyphs) {
            result = std::max(result, glyph.position.x + glyph.glyph.advance);
        }
        return result;
    }
    const sf::FloatRect bounds = text.getLocalBounds();
    return bounds.position.x + bounds.size.x;
}

float RichText::getLineAdvance(const TextStyle& style) const {
    const unsigned int characterSize =
        ludork::engine::text_detail::scaledCharacterSize(
            style.characterSize.value_or(
                static_cast<unsigned int>(std::max(1, defaultFontSize))));
    return config_->font->getLineSpacing(characterSize) *
           style.lineSpacing.value_or(1.0f);
}

void RichText::refreshSegmentColours() {
    for (Segment& segment : segments_) {
        applySegmentColour(*segment.text, segment.style);
    }
}

void RichText::applySegmentColour(sf::Text& text,
                                  const TextStyle& style) const {
    const sf::Color fill = style.fillColor.value_or(sf::Color::White);
    const sf::Color outline = style.outlineColor.value_or(sf::Color::Black);
    text.setFillColor(modulateColour(fill, colour_));
    text.setOutlineColor(modulateColour(outline, colour_));
}

void RichText::invalidateEffects() {
    effects_->data.dirty = true;
}

void RichText::syncDisplayScale() const {
    if (displayScale_ != Scale) {
        const_cast<RichText*>(this)->refreshDisplayScale();
    }
}

void RichText::ensureEffects() const {
    if (!effects_->data.dirty) {
        return;
    }
    std::vector<ludork::engine::text_effects::Source> sources;
    sources.reserve(segments_.size());
    for (const Segment& segment : segments_) {
        sources.push_back(
            {segment.text.get(),
             segment.style.fillColor.value_or(sf::Color::White),
             segment.style.outlineColor.value_or(sf::Color::Black)});
    }
    ludork::engine::text_effects::rebuild(effects_->data, sources,
                                          config_->glow, config_->gradient);
}
