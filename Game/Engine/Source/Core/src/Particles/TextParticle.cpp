#include <Particles/TextParticle.hpp>
#include <Particles/ParticleBase.hpp>
#include <UI/PlainText.hpp>
#include <UI/PlainTextConfig.hpp>

#include <Particles/ParticleSystem.hpp>
#include <EngineState.hpp>

#include <utility>

TextParticle::TextParticle(
    std::shared_ptr<ParticleSystem> parent,
    std::function<void(float, float, TextParticle*)> moveFunction,
    float countTime, const std::string& text,
    std::shared_ptr<PlainTextConfig> config, bool logicalCoordinates)
    : ParticleBase(std::move(parent), nullptr, countTime),
      text_(std::make_shared<PlainText>(std::move(config), text)),
      logicalCoordinates_(logicalCoordinates) {
    if (moveFunction) {
        moveFunction_ = [moveFunction = std::move(moveFunction)](
                            float deltaTime, float countTime,
                            ParticleBase* particle) {
            moveFunction(deltaTime, countTime,
                         ludork::Cast<TextParticle>(particle));
        };
    }
}

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
