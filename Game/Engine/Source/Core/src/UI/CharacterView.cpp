#include <UI/CharacterView.hpp>
#include <UI/FunctionalImage.hpp>

#include <CoreShared/SpriteVisuals.hpp>

#include <SFML/Graphics/Glsl.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

CharacterView::CharacterView(std::shared_ptr<sf::Texture> texture,
                             std::optional<sf::IntRect> frameRect,
                             std::optional<sf::Vector2f> size, bool animatable,
                             float switchInterval, std::string material,
                             RuntimeData::Map materialParameters)
    : FunctionalImage(texture,
                      frameRect.value_or(texture ? fullTextureRect(*texture)
                                                 : sf::IntRect{})),
      characterTexture_(std::move(texture)),
      initialFrameRect_(frameRect.value_or(
          characterTexture_ ? fullTextureRect(*characterTexture_)
                            : sf::IntRect{})),
      frameRect_(initialFrameRect_),
      size_(size.value_or(defaultViewSize(frameRect_))),
      animatable_(animatable),
      switchInterval_(switchInterval),
      material_(std::move(material)),
      materialParameters_(std::move(materialParameters)) {
    if (characterTexture_ == nullptr) {
        throw std::invalid_argument("CharacterView texture must not be null");
    }
    materialResource_ =
        ludork::engine::materials::MaterialResource::load(material_);
    materialResource_->validateParameters(materialParameters_);
}

void CharacterView::setCharacter(std::shared_ptr<sf::Texture> texture,
                                 const sf::IntRect& frameRect,
                                 const sf::Vector2f& characterScale,
                                 bool animatable, float switchInterval,
                                 const std::string& material,
                                 const RuntimeData::Map& materialParameters) {
    if (texture == nullptr) {
        throw std::invalid_argument("CharacterView texture must not be null");
    }
    characterTexture_ = std::move(texture);
    SpriteBase::setTexture(characterTexture_, true);
    initialFrameRect_ = frameRect;
    frameRect_ = frameRect;
    SpriteBase::setTextureRect(frameRect_);
    characterScale_ = characterScale;
    animatable_ = animatable;
    switchInterval_ = switchInterval;
    switchTimer_ = 0.0f;
    materialTime_ = 0.0f;
    setMaterial(material);
    setMaterialParameters(materialParameters);
}

void CharacterView::setCharacterTexture(std::shared_ptr<sf::Texture> texture) {
    if (texture == nullptr) {
        throw std::invalid_argument("CharacterView texture must not be null");
    }
    characterTexture_ = std::move(texture);
    SpriteBase::setTexture(characterTexture_, true);
    resetFrameRectToTexture();
    materialTime_ = 0.0f;
}

sf::Vector2f CharacterView::getCharacterScale() const {
    return characterScale_;
}

void CharacterView::setCharacterScale(const sf::Vector2f& scale) {
    characterScale_ = scale;
}

bool CharacterView::getAnimatable() const {
    return animatable_;
}

void CharacterView::setAnimatable(bool animatable) {
    animatable_ = animatable;
    if (!animatable_) {
        switchTimer_ = 0.0f;
    }
}

float CharacterView::getSwitchInterval() const {
    return switchInterval_;
}

void CharacterView::setSwitchInterval(float switchInterval) {
    switchInterval_ = switchInterval;
    switchTimer_ = 0.0f;
}

float CharacterView::getSwitchTimer() const {
    return switchTimer_;
}

sf::IntRect CharacterView::getFrameRect() const {
    return frameRect_;
}

void CharacterView::setFrameRect(const sf::IntRect& frameRect) {
    initialFrameRect_ = frameRect;
    frameRect_ = frameRect;
    SpriteBase::setTextureRect(frameRect_);
    switchTimer_ = 0.0f;
}

void CharacterView::resetFrameRectToTexture() {
    setFrameRect(fullTextureRect(*characterTexture_));
}

const std::string& CharacterView::getMaterial() const {
    return material_;
}

void CharacterView::setMaterial(const std::string& material) {
    const auto resource =
        ludork::engine::materials::MaterialResource::load(material);
    if (material_ != material) {
        materialParameters_.clear();
        material_ = material;
    }
    materialResource_ = resource;
}

RuntimeData::Map CharacterView::getMaterialParameters() const {
    return materialParameters_;
}

void CharacterView::setMaterialParameters(const RuntimeData::Map& parameters) {
    materialResource_->validateParameters(parameters);
    materialParameters_ = parameters;
}

void CharacterView::resetAnimation() {
    switchTimer_ = 0.0f;
    materialTime_ = 0.0f;
    frameRect_ = initialFrameRect_;
    SpriteBase::setTextureRect(frameRect_);
}

sf::Vector2f CharacterView::getSize() const {
    return size_;
}

sf::FloatRect CharacterView::getLocalBounds() const {
    return {{0.0f, 0.0f}, size_};
}

sf::FloatRect CharacterView::getGlobalBounds() const {
    return getTransform().transformRect(getLocalBounds());
}

void CharacterView::resize(const sf::Vector2f& size) {
    size_ = {std::max(1.0f, size.x), std::max(1.0f, size.y)};
}

void CharacterView::update(float deltaTime) {
    FunctionalBase::update(deltaTime);
    if (deltaTime <= 0.0f) {
        return;
    }
    materialTime_ += deltaTime;
    const int frameWidth = frameRect_.size.x;
    const unsigned int textureWidth = characterTexture_->getSize().x;
    if (!animatable_ || switchInterval_ <= 0.0f || frameWidth <= 0 ||
        textureWidth <= static_cast<unsigned int>(frameWidth)) {
        return;
    }
    switchTimer_ += deltaTime;
    while (switchTimer_ >= switchInterval_) {
        switchTimer_ -= switchInterval_;
        frameRect_ = ludork::engine::sprite_visuals::nextAnimationRect(
            frameRect_, textureWidth);
    }
    SpriteBase::setTextureRect(frameRect_);
}

void CharacterView::draw(sf::RenderTarget& target,
                         sf::RenderStates states) const {
    if (!getVisible()) {
        return;
    }
    _applyRenderStates(states);
    states.blendMode = getRenderStates().blendMode;
    const sf::Sprite sprite =
        finalSprite(*characterTexture_, frameRect_, presentedColour());
    ludork::engine::materials::MaterialProgramCache::Context context;
    context.textureSize = characterTexture_->getSize();
    context.textureRect = frameRect_;
    context.time = materialTime_;
    context.targetSize = target.getSize();
    context.worldTransform = states.transform * sprite.getTransform();
    const auto program = materialPrograms_.bind(
        *materialResource_, materialParameters_, nullptr, {},
        ludork::engine::materials::MaterialCompiler::Pass::Color, context);
    states.shader = program.get();
    target.draw(sprite, states);
}

sf::IntRect CharacterView::fullTextureRect(const sf::Texture& texture) {
    const sf::Vector2u size = texture.getSize();
    return {{0, 0}, {static_cast<int>(size.x), static_cast<int>(size.y)}};
}

sf::Vector2f CharacterView::defaultViewSize(const sf::IntRect& frameRect) {
    return {static_cast<float>(std::max(1, std::abs(frameRect.size.x))),
            static_cast<float>(std::max(1, std::abs(frameRect.size.y)))};
}

sf::Vector2f CharacterView::nonZeroScale(const sf::Vector2f& scale) {
    constexpr float minimum = 0.01f;
    const auto component = [](float value) {
        if (std::abs(value) >= minimum) {
            return value;
        }
        return value < 0.0f ? -minimum : minimum;
    };
    return {component(scale.x), component(scale.y)};
}

sf::Sprite CharacterView::finalSprite(const sf::Texture& texture,
                                      const sf::IntRect& textureRect,
                                      const sf::Color& colour) const {
    const sf::Vector2f displayScale = nonZeroScale(characterScale_);
    const float displayWidth =
        std::max(1.0f, std::abs(textureRect.size.x * displayScale.x));
    const float displayHeight =
        std::max(1.0f, std::abs(textureRect.size.y * displayScale.y));
    const float fit =
        std::min({1.0f, size_.x / displayWidth, size_.y / displayHeight});
    sf::Sprite sprite(texture, textureRect);
    sprite.setColor(colour);
    sprite.setOrigin({static_cast<float>(textureRect.size.x) / 2.0f,
                      static_cast<float>(textureRect.size.y) / 2.0f});
    sprite.setScale(displayScale * fit);
    sprite.setPosition(size_ / 2.0f);
    return sprite;
}
