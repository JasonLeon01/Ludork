#include <UI/Text.hpp>

#include <Runtime/EngineState.hpp>
#include <UI/UIState.hpp>
#include <Utils/Inner.hpp>
#include <Utils/ShaderLoader.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/System/String.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {

constexpr std::size_t CurveSampleCount = 256;
constexpr const char* DefaultTextEffectsShader = "Global/TextEffects.frag";

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

bool glowEnabled(const TextGlowConfig& glow) {
    return glow.enabled && glow.radius > 0.0f && glow.intensity > 0.0f &&
           glow.color.a > 0;
}

bool effectsEnabled(const TextGlowConfig& glow,
                    const TextGradientConfig& gradient) {
    return glowEnabled(glow) || gradient.enabled;
}

float glowRadiusPixels(const TextGlowConfig& glow) {
    return glowEnabled(glow) ? std::max(0.0f, glow.radius * Scale) : 0.0f;
}

sf::FloatRect expandedForGlow(const sf::FloatRect& bounds,
                              const TextGlowConfig& glow) {
    const float radius = glowRadiusPixels(glow);
    return {{bounds.position.x - radius, bounds.position.y - radius},
            {bounds.size.x + radius * 2.0f, bounds.size.y + radius * 2.0f}};
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

void warnTextEffectOnce(const std::string& key, const std::string& message) {
    static std::unordered_set<std::string> warnings;
    if (warnings.insert(key).second) {
        std::cerr << message << '\n';
    }
}

std::shared_ptr<sf::Shader> loadTextEffectsShader() {
    static std::unordered_map<std::string, std::weak_ptr<sf::Shader>>
        shaderCache;
    static std::unordered_set<std::string> failedShaders;
    const std::string path = DefaultTextEffectsShader;
    if (!sf::Shader::isAvailable()) {
        warnTextEffectOnce(
            "Text.shaderUnavailable",
            "Text effects are disabled because shaders are unavailable");
        return nullptr;
    }
    if (failedShaders.find(path) != failedShaders.end()) {
        return nullptr;
    }
    const auto cached = shaderCache.find(path);
    if (cached != shaderCache.end()) {
        const std::shared_ptr<sf::Shader> shader = cached->second.lock();
        if (shader != nullptr) {
            return shader;
        }
    }
    ShaderLoadResult loaded =
        ShaderLoader::load(path, sf::Shader::Type::Fragment);
    if (!loaded) {
        failedShaders.insert(path);
        warnTextEffectOnce("Text.shaderLoad:" + path, loaded.error);
        return nullptr;
    }
    shaderCache[path] = loaded.shader;
    return loaded.shader;
}

std::uint8_t curveTextureChannel(float value) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0l, 255l));
}

std::unique_ptr<sf::Texture> buildCurveTexture(
    const std::shared_ptr<Vector4Curve>& curve) {
    sf::Image image({static_cast<unsigned int>(CurveSampleCount), 1u},
                    sf::Color::White);
    for (std::size_t index = 0; index < CurveSampleCount; ++index) {
        const float input = static_cast<float>(index) /
                            static_cast<float>(CurveSampleCount - 1);
        const std::array<float, 4> output =
            curve == nullptr
                ? std::array<float, 4>{255.0f, 255.0f, 255.0f, 255.0f}
                : curve->evaluate(input);
        image.setPixel({static_cast<unsigned int>(index), 0u},
                       sf::Color(curveTextureChannel(output[0]),
                                 curveTextureChannel(output[1]),
                                 curveTextureChannel(output[2]),
                                 curveTextureChannel(output[3])));
    }
    std::unique_ptr<sf::Texture> texture = std::make_unique<sf::Texture>(image);
    texture->setSmooth(true);
    return texture;
}

struct EffectTextSource {
    const sf::Text* text = nullptr;
    sf::Color fillColor = sf::Color::White;
    sf::Color outlineColor = sf::Color::Black;
    sf::Transform transform;
};

struct TextEffectCacheData {
    bool dirty = true;
    sf::FloatRect pixelBounds;
    sf::Vector2f contentMinimum;
    sf::Vector2f contentMaximum;
    std::unique_ptr<sf::RenderTexture> fill;
    std::unique_ptr<sf::RenderTexture> outline;
    std::unique_ptr<sf::Texture> curve;
    std::unique_ptr<sf::Sprite> sprite;
    std::shared_ptr<sf::Shader> shader;
};

sf::FloatRect combinedTextBounds(const std::vector<EffectTextSource>& sources,
                                 bool includeOutline) {
    bool hasBounds = false;
    float minimumX = 0.0f;
    float minimumY = 0.0f;
    float maximumX = 0.0f;
    float maximumY = 0.0f;
    for (const EffectTextSource& source : sources) {
        if (source.text == nullptr) {
            continue;
        }
        sf::Text measuredText = *source.text;
        if (!includeOutline) {
            measuredText.setOutlineThickness(0.0f);
        }
        const sf::FloatRect bounds =
            source.transform.transformRect(measuredText.getGlobalBounds());
        if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) {
            continue;
        }
        const float right = bounds.position.x + bounds.size.x;
        const float bottom = bounds.position.y + bounds.size.y;
        if (!hasBounds) {
            minimumX = bounds.position.x;
            minimumY = bounds.position.y;
            maximumX = right;
            maximumY = bottom;
            hasBounds = true;
            continue;
        }
        minimumX = std::min(minimumX, bounds.position.x);
        minimumY = std::min(minimumY, bounds.position.y);
        maximumX = std::max(maximumX, right);
        maximumY = std::max(maximumY, bottom);
    }
    if (!hasBounds) {
        return {};
    }
    return {{minimumX, minimumY}, {maximumX - minimumX, maximumY - minimumY}};
}

void clearEffectCache(TextEffectCacheData& cache) {
    cache.fill.reset();
    cache.outline.reset();
    cache.curve.reset();
    cache.sprite.reset();
    cache.shader.reset();
    cache.pixelBounds = {};
    cache.contentMinimum = {};
    cache.contentMaximum = {};
    cache.dirty = false;
}

void rebuildEffectCache(TextEffectCacheData& cache,
                        const std::vector<EffectTextSource>& sources,
                        const TextGlowConfig& glow,
                        const TextGradientConfig& gradient) {
    const sf::FloatRect renderBounds = combinedTextBounds(sources, true);
    const sf::FloatRect fillBounds = combinedTextBounds(sources, false);
    if (renderBounds.size.x <= 0.0f || renderBounds.size.y <= 0.0f ||
        fillBounds.size.x <= 0.0f || fillBounds.size.y <= 0.0f) {
        clearEffectCache(cache);
        return;
    }

    const std::shared_ptr<sf::Shader> shader = loadTextEffectsShader();
    if (shader == nullptr) {
        clearEffectCache(cache);
        return;
    }

    const float padding = std::ceil(glowRadiusPixels(glow)) + 2.0f;
    const float left = std::floor(renderBounds.position.x - padding);
    const float top = std::floor(renderBounds.position.y - padding);
    const float right =
        std::ceil(renderBounds.position.x + renderBounds.size.x + padding);
    const float bottom =
        std::ceil(renderBounds.position.y + renderBounds.size.y + padding);
    const sf::Vector2u textureSize{
        static_cast<unsigned int>(std::max(1.0f, right - left)),
        static_cast<unsigned int>(std::max(1.0f, bottom - top)),
    };
    const unsigned int maximumSize = sf::Texture::getMaximumSize();
    if (textureSize.x > maximumSize || textureSize.y > maximumSize) {
        warnTextEffectOnce(
            "Text.effectCacheMaximum",
            "Text effects were skipped because the cache exceeds the maximum "
            "texture size");
        clearEffectCache(cache);
        return;
    }

    cache.pixelBounds = {
        {left, top},
        {static_cast<float>(textureSize.x), static_cast<float>(textureSize.y)}};
    cache.contentMinimum = {
        (fillBounds.position.x - left) / static_cast<float>(textureSize.x),
        (fillBounds.position.y - top) / static_cast<float>(textureSize.y),
    };
    cache.contentMaximum = {
        (fillBounds.position.x + fillBounds.size.x - left) /
            static_cast<float>(textureSize.x),
        (fillBounds.position.y + fillBounds.size.y - top) /
            static_cast<float>(textureSize.y),
    };

    cache.fill = std::make_unique<sf::RenderTexture>(textureSize);
    cache.outline = std::make_unique<sf::RenderTexture>(textureSize);
    cache.fill->clear(sf::Color::Transparent);
    cache.outline->clear(sf::Color::Transparent);
    sf::RenderStates cacheStates;
    cacheStates.transform.translate({-left, -top});

    for (const EffectTextSource& source : sources) {
        if (source.text == nullptr) {
            continue;
        }
        sf::RenderStates sourceStates = cacheStates;
        sourceStates.transform.combine(source.transform);
        sf::Text fillText = *source.text;
        fillText.setFillColor(source.fillColor);
        fillText.setOutlineThickness(0.0f);
        cache.fill->draw(fillText, sourceStates);

        if (source.text->getOutlineThickness() != 0.0f &&
            source.outlineColor.a != 0) {
            sf::Text outlineText = *source.text;
            outlineText.setFillColor(sf::Color::Transparent);
            outlineText.setOutlineColor(source.outlineColor);
            cache.outline->draw(outlineText, sourceStates);
        }
    }

    cache.fill->display();
    cache.outline->display();
    cache.curve = buildCurveTexture(gradient.curve);
    cache.sprite = std::make_unique<sf::Sprite>(cache.fill->getTexture());
    cache.sprite->setPosition(cache.pixelBounds.position);
    cache.shader = shader;
    cache.dirty = false;
}

bool drawEffectCache(TextEffectCacheData& cache, sf::RenderTarget& target,
                     sf::RenderStates states, const sf::Color& colour,
                     const TextGlowConfig& glow,
                     const TextGradientConfig& gradient) {
    if (cache.sprite == nullptr || cache.shader == nullptr ||
        cache.fill == nullptr || cache.outline == nullptr ||
        cache.curve == nullptr) {
        return false;
    }

    cache.sprite->setColor(colour);
    cache.shader->setUniform("texture", cache.fill->getTexture());
    cache.shader->setUniform("outlineTexture", cache.outline->getTexture());
    cache.shader->setUniform("curveTexture", *cache.curve);
    cache.shader->setUniform("textureSize", cache.pixelBounds.size);
    cache.shader->setUniform("contentMinimum", cache.contentMinimum);
    cache.shader->setUniform("contentMaximum", cache.contentMaximum);
    cache.shader->setUniform("gradientEnabled", gradient.enabled);
    cache.shader->setUniform("gradientDirection",
                             gradient.direction == "horizontal" ? 1 : 0);
    cache.shader->setUniform("glowEnabled", glowEnabled(glow));
    cache.shader->setUniform("glowColor", sf::Glsl::Vec4(glow.color));
    cache.shader->setUniform("glowRadius", glowRadiusPixels(glow));
    cache.shader->setUniform("glowIntensity",
                             std::clamp(glow.intensity, 0.0f, 1.0f));
    states.shader = cache.shader.get();
    target.draw(*cache.sprite, states);
    return true;
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
    TextEffectCacheData data;
};

struct RichText::EffectCache {
    TextEffectCacheData data;
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
      effects_(std::make_unique<EffectCache>()) {
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
    return expandedForGlow(
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
    return ControlBase::getOrigin() / Scale;
}

void PlainText::setOrigin(const sf::Vector2f& origin) {
    ControlBase::setOrigin(origin * Scale);
}

sf::Color PlainText::getColour() const {
    return colour_;
}

void PlainText::setColour(const sf::Color& colour) {
    colour_ = colour;
    refreshDirectColours();
}

void PlainText::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    _applyRenderStates(states);
    if (!getVisible()) {
        return;
    }
    sf::RenderStates textStates = states;
    textStates.transform.combine(customSlantTransform(*config_));
    if (!effectsEnabled(config_->glow, config_->gradient)) {
        target.draw(text_, textStates);
        return;
    }
    ensureEffects();
    if (!drawEffectCache(effects_->data, target, states, colour_, config_->glow,
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

void PlainText::ensureEffects() const {
    if (!effects_->data.dirty) {
        return;
    }
    rebuildEffectCache(effects_->data,
                       {{&text_, config_->fillColor, config_->outline.color,
                         customSlantTransform(*config_)}},
                       config_->glow, config_->gradient);
}

RichText::RichText(std::shared_ptr<RichTextConfig> config,
                   const std::string& text)
    : config_(snapshotConfig(config)),
      localBounds_({0.0f, 0.0f}, {0.0f, 0.0f}),
      effects_(std::make_unique<EffectCache>()) {
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
    return expandedForGlow(localBounds_, config_->glow);
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
    return ControlBase::getOrigin() / Scale;
}

void RichText::setOrigin(const sf::Vector2f& origin) {
    ControlBase::setOrigin(origin * Scale);
}

void RichText::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    _applyRenderStates(states);
    if (!getVisible()) {
        return;
    }
    if (!effectsEnabled(config_->glow, config_->gradient)) {
        for (const Segment& segment : segments_) {
            target.draw(*segment.text, states);
        }
        return;
    }
    ensureEffects();
    if (drawEffectCache(effects_->data, target, states, colour_, config_->glow,
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

void RichText::ensureEffects() const {
    if (!effects_->data.dirty) {
        return;
    }
    std::vector<EffectTextSource> sources;
    sources.reserve(segments_.size());
    for (const Segment& segment : segments_) {
        sources.push_back(
            {segment.text.get(),
             segment.style.fillColor.value_or(sf::Color::White),
             segment.style.outlineColor.value_or(sf::Color::Black)});
    }
    rebuildEffectCache(effects_->data, sources, config_->glow,
                       config_->gradient);
}
