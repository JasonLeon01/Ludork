#include <Particles/TextParticle.hpp>

#include <Particles/ParticleSystem.hpp>
#include <Runtime/EngineState.hpp>

#include <utility>

TextParticle::TextParticle(
    std::shared_ptr<ParticleSystem> parent,
    std::function<void(float, float, ParticleBase*)> moveFunction,
    float countTime, const std::string& text,
    std::shared_ptr<PlainTextConfig> config, bool logicalCoordinates)
    : ParticleBase(std::move(parent), std::move(moveFunction), countTime),
      text_(std::make_shared<PlainText>(std::move(config), text)),
      logicalCoordinates_(logicalCoordinates) {}

std::shared_ptr<PlainTextConfig> TextParticle::getConfig() const {
    return text_->getConfig();
}

void TextParticle::setString(const std::string& text) {
    text_->setString(text);
}

std::string TextParticle::getString() const {
    return text_->getString();
}

void TextParticle::setColour(const sf::Color& colour) {
    text_->setColour(colour);
}

sf::Color TextParticle::getColour() const {
    return text_->getColour();
}

sf::FloatRect TextParticle::getLocalBounds() const {
    const sf::FloatRect bounds = text_->getLocalBounds();
    if (logicalCoordinates_) {
        return bounds;
    }
    const float scale = engineState().getScale();
    return {bounds.position * scale, bounds.size * scale};
}

sf::FloatRect TextParticle::getGlobalBounds() const {
    return getTransform().transformRect(getLocalBounds());
}

void TextParticle::draw(sf::RenderTarget& target,
                        sf::RenderStates states) const {
    states.transform.combine(getTransform());
    if (logicalCoordinates_) {
        const float scale = engineState().getScale();
        if (scale > 0.0f) {
            states.transform.scale({1.0f / scale, 1.0f / scale});
        }
    }
    target.draw(*text_, states);
}
