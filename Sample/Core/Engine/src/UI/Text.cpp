#include <UI/Text.hpp>

#include <Runtime/EngineState.hpp>
#include <UI/TextEffects.hpp>
#include <UI/UIState.hpp>
#include <Utils/Inner.hpp>

#include <SFML/Graphics/Transform.hpp>
#include <SFML/System/String.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {

sf::String toSfString(const std::string& value) {
    return sf::String::fromUtf8(value.begin(), value.end());
}

std::string toUtf8String(const sf::String& value) {
    const sf::U8String bytes = value.toUtf8();
    return {bytes.begin(), bytes.end()};
}

unsigned int scaledCharacterSize(unsigned int characterSize) {
    return static_cast<unsigned int>(
        std::max(1.0f, std::floor(static_cast<float>(characterSize) * Scale)));
}

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

sf::Color modulateColour(const sf::Color& baseColour,
                         const sf::Color& factorColour) {
    return {
        static_cast<std::uint8_t>(baseColour.r * factorColour.r / 255),
        static_cast<std::uint8_t>(baseColour.g * factorColour.g / 255),
        static_cast<std::uint8_t>(baseColour.b * factorColour.b / 255),
        static_cast<std::uint8_t>(baseColour.a * factorColour.a / 255),
    };
}

void validateGradient(const TextGradientConfig& gradient) {
    if (gradient.direction != "vertical" &&
        gradient.direction != "horizontal") {
        throw std::invalid_argument(
            "Text gradient direction must be vertical or horizontal");
    }
}

sf::Transform customSlantTransform(const PlainTextConfig& config) {
    if ((config.style & static_cast<std::uint32_t>(sf::Text::Italic)) != 0 ||
        !std::isfinite(config.slantAngle)) {
        return {};
    }
    const float angle = std::clamp(config.slantAngle, -45.0f, 45.0f);
    if (std::abs(angle) <= 0.0001f) {
        return {};
    }
    constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;
    const float shear = std::tan(angle * DegreesToRadians);
    return {1.0f, -shear, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
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

struct PlainText::EffectCache {
    ludork::engine::text_effects::Cache data;
};

struct RichText::EffectCache {
    ludork::engine::text_effects::Cache data;
};

void TextStyle::enableStyle(sf::Text& text) const {
    if (characterSize.has_value()) {
        text.setCharacterSize(scaledCharacterSize(*characterSize));
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

PlainText::PlainText(std::shared_ptr<PlainTextConfig> config,
                     const std::string& text)
    : config_(snapshotConfig(config)),
      text_(*config_->font, toSfString(text),
            scaledCharacterSize(std::max(1u, config_->characterSize))),
      effects_(std::make_unique<EffectCache>()),
      displayScale_(Scale) {
    applyConfig();
}

PlainText::~PlainText() = default;

std::shared_ptr<PlainTextConfig> PlainText::getConfig() const {
    return snapshotConfig(*config_);
}

unsigned int PlainText::getCharacterSize() const {
    return config_->characterSize;
}

void PlainText::setString(const std::string& text) {
    text_.setString(toSfString(text));
    invalidateEffects();
}

std::string PlainText::getString() const {
    return toUtf8String(text_.getString());
}

sf::FloatRect PlainText::getPixelBounds() const {
    syncDisplayScale();
    return ludork::engine::text_effects::expandedBounds(
        customSlantTransform(*config_).transformRect(text_.getLocalBounds()),
        config_->glow);
}

sf::FloatRect PlainText::getLocalBounds() const {
    const sf::FloatRect bounds = getPixelBounds();
    return {bounds.position / Scale, bounds.size / Scale};
}

sf::FloatRect PlainText::getGlobalBounds() const {
    return getTransform().transformRect(getLocalBounds());
}

sf::Vector2f PlainText::getSize() const {
    return getPixelBounds().size / Scale;
}

sf::Vector2f PlainText::getOrigin() const {
    syncDisplayScale();
    return ControlBase::getOrigin() / Scale;
}

void PlainText::setOrigin(const sf::Vector2f& origin) {
    syncDisplayScale();
    ControlBase::setOrigin(origin * Scale);
}

sf::Color PlainText::getColour() const {
    return colour_;
}

void PlainText::setColour(const sf::Color& colour) {
    colour_ = colour;
    refreshDirectColours();
}

void PlainText::refreshDisplayScale() {
    if (displayScale_ != Scale) {
        const sf::Vector2f logicalOrigin =
            ControlBase::getOrigin() / displayScale_;
        displayScale_ = Scale;
        applyConfig();
        setOrigin(logicalOrigin);
    }
    ControlBase::refreshDisplayScale();
}

void PlainText::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    syncDisplayScale();
    _applyRenderStates(states);
    if (!getVisible()) {
        return;
    }
    sf::RenderStates textStates = states;
    textStates.transform.combine(customSlantTransform(*config_));
    if (!ludork::engine::text_effects::enabled(config_->glow,
                                               config_->gradient)) {
        target.draw(text_, textStates);
        return;
    }
    ensureEffects();
    if (!ludork::engine::text_effects::draw(effects_->data, target, states,
                                            colour_, config_->glow,
                                            config_->gradient)) {
        target.draw(text_, textStates);
    }
}

const PlainTextConfig& PlainText::configReference(
    const std::shared_ptr<PlainTextConfig>& config) {
    if (config == nullptr) {
        throw std::invalid_argument("PlainText config must not be null");
    }
    if (config->type != "plainTextConfig") {
        throw std::invalid_argument(
            "PlainText requires a plainTextConfig asset");
    }
    if (config->font == nullptr) {
        throw std::invalid_argument(
            "PlainTextConfig resolved font must not be null");
    }
    validateGradient(config->gradient);
    return *config;
}

std::shared_ptr<PlainTextConfig> PlainText::snapshotConfig(
    const std::shared_ptr<PlainTextConfig>& config) {
    return snapshotConfig(configReference(config));
}

std::shared_ptr<PlainTextConfig> PlainText::snapshotConfig(
    const PlainTextConfig& config) {
    std::shared_ptr<PlainTextConfig> snapshot =
        std::make_shared<PlainTextConfig>(config);
    if (config.gradient.curve != nullptr) {
        snapshot->gradient.curve =
            Vector4Curve::fromData(config.gradient.curve->toData());
    }
    return snapshot;
}

void PlainText::applyConfig() {
    text_.setCharacterSize(
        scaledCharacterSize(std::max(1u, config_->characterSize)));
    text_.setStyle(config_->style);
    text_.setLetterSpacing(config_->letterSpacing);
    text_.setLineSpacing(config_->lineSpacing);
    text_.setLineAlignment(config_->lineAlignment);
    text_.setOutlineThickness(std::max(0.0f, config_->outline.thickness) *
                              Scale);
    refreshDirectColours();
    invalidateEffects();
}

void PlainText::refreshDirectColours() {
    text_.setFillColor(modulateColour(config_->fillColor, colour_));
    text_.setOutlineColor(modulateColour(config_->outline.color, colour_));
}

void PlainText::invalidateEffects() {
    effects_->data.dirty = true;
}

void PlainText::syncDisplayScale() const {
    if (displayScale_ != Scale) {
        const_cast<PlainText*>(this)->refreshDisplayScale();
    }
}

void PlainText::ensureEffects() const {
    if (!effects_->data.dirty) {
        return;
    }
    ludork::engine::text_effects::rebuild(
        effects_->data,
        {{&text_, config_->fillColor, config_->outline.color,
          customSlantTransform(*config_)}},
        config_->glow, config_->gradient);
}

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
    validateGradient(config->gradient);
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
    return ::modulateColour(baseColour, factorColour);
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
    std::unique_ptr<sf::Text> result =
        std::make_unique<sf::Text>(*config_->font, toSfString(text),
                                   scaledCharacterSize(*style.characterSize));
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
        scaledCharacterSize(style.characterSize.value_or(
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
