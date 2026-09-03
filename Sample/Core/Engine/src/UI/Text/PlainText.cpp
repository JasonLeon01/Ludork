#include "TextCommon.hpp"

#include <Runtime/EngineState.hpp>
#include <UI/TextEffects.hpp>

#include <SFML/Graphics/Transform.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

std::string toUtf8String(const sf::String& value) {
    const sf::U8String bytes = value.toUtf8();
    return {bytes.begin(), bytes.end()};
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

}  // namespace

struct PlainText::EffectCache {
    ludork::engine::text_effects::Cache data;
};

PlainText::PlainText(std::shared_ptr<PlainTextConfig> config,
                     const std::string& text)
    : config_(snapshotConfig(config)),
      text_(*config_->font, ludork::engine::text_detail::toSfString(text),
            ludork::engine::text_detail::scaledCharacterSize(
                std::max(1u, config_->characterSize))),
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
    text_.setString(ludork::engine::text_detail::toSfString(text));
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
                                            presentedColour(), config_->glow,
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
    ludork::engine::text_detail::validateGradient(config->gradient);
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
    text_.setCharacterSize(ludork::engine::text_detail::scaledCharacterSize(
        std::max(1u, config_->characterSize)));
    text_.setStyle(config_->style);
    text_.setLetterSpacing(config_->letterSpacing);
    text_.setLineSpacing(config_->lineSpacing);
    text_.setLineAlignment(config_->lineAlignment);
    text_.setOutlineThickness(std::max(0.0f, config_->outline.thickness) *
                              Scale);
    refreshDirectColours();
    invalidateEffects();
}

sf::Color PlainText::presentedColour() const {
    return modulatePresentationColour(colour_);
}

void PlainText::refreshDirectColours() {
    const sf::Color colour = presentedColour();
    text_.setFillColor(ludork::engine::text_detail::modulateColour(
        config_->fillColor, colour));
    text_.setOutlineColor(ludork::engine::text_detail::modulateColour(
        config_->outline.color, colour));
}

void PlainText::_refreshPresentationColour() {
    refreshDirectColours();
    invalidateEffects();
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
