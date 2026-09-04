#include <UI/CharacterView.hpp>

#include "../Gameplay/Actor/VisualRuntime.hpp"

#include <SFML/Graphics/Glsl.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

constexpr float HueEpsilon = 0.0001f;

}  // namespace

CharacterView::CharacterView(std::shared_ptr<sf::Texture> texture,
                             std::optional<sf::IntRect> frameRect,
                             std::optional<sf::Vector2f> size, bool animatable,
                             float switchInterval, std::string shaderPath,
                             float hue)
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
      shaderPath_(std::move(shaderPath)),
      hue_(normaliseHue(hue)) {
    if (characterTexture_ == nullptr) {
        throw std::invalid_argument("CharacterView texture must not be null");
    }
    applyShaderPath();
}

void CharacterView::setCharacter(std::shared_ptr<sf::Texture> texture,
                                 const sf::IntRect& frameRect,
                                 const sf::Vector2f& characterScale,
                                 bool animatable, float switchInterval,
                                 const std::string& shaderPath, float hue) {
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
    shaderTime_ = 0.0f;
    shaderPath_ = shaderPath;
    hue_ = normaliseHue(hue);
    applyShaderPath();
}

void CharacterView::setCharacterTexture(std::shared_ptr<sf::Texture> texture) {
    if (texture == nullptr) {
        throw std::invalid_argument("CharacterView texture must not be null");
    }
    characterTexture_ = std::move(texture);
    SpriteBase::setTexture(characterTexture_, true);
    resetFrameRectToTexture();
    shaderTime_ = 0.0f;
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

const std::string& CharacterView::getShaderPath() const {
    return shaderPath_;
}

void CharacterView::setShaderPath(const std::string& shaderPath) {
    shaderPath_ = shaderPath;
    shaderTime_ = 0.0f;
    applyShaderPath();
}

float CharacterView::getHue() const {
    return hue_;
}

void CharacterView::setHue(float hue) {
    hue_ = normaliseHue(hue);
    applyShaderPath();
}

void CharacterView::resetAnimation() {
    switchTimer_ = 0.0f;
    shaderTime_ = 0.0f;
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
    shaderTime_ += deltaTime;
    const int frameWidth = frameRect_.size.x;
    const unsigned int textureWidth = characterTexture_->getSize().x;
    if (!animatable_ || switchInterval_ <= 0.0f || frameWidth <= 0 ||
        textureWidth <= static_cast<unsigned int>(frameWidth)) {
        return;
    }
    switchTimer_ += deltaTime;
    while (switchTimer_ >= switchInterval_) {
        switchTimer_ -= switchInterval_;
        frameRect_ = ludork::engine::actor_impl::nextAnimationRect(
            frameRect_, textureWidth);
    }
    SpriteBase::setTextureRect(frameRect_);
}

void CharacterView::draw(sf::RenderTarget& target,
                         sf::RenderStates states) const {
    _applyRenderStates(states);
    states.blendMode = getRenderStates().blendMode;
    const sf::Color colour = shaderError_
                                 ? sf::Color(255, 0, 255, presentedColour().a)
                                 : presentedColour();
    if (shaderError_) {
        target.draw(finalSprite(*characterTexture_, frameRect_, colour),
                    states);
        return;
    }

    const bool hasHue = hueShader_ != nullptr && !neutralHue(hue_);
    if (shader_ == nullptr) {
        if (hasHue) {
            hueShader_->setUniform("screenTex", sf::Shader::CurrentTexture);
            hueShader_->setUniform("hue", hue_);
            states.shader = hueShader_.get();
        }
        target.draw(finalSprite(*characterTexture_, frameRect_, colour),
                    states);
        return;
    }

    applyActorShaderUniforms();
    if (!hasHue) {
        states.shader = shader_.get();
        target.draw(finalSprite(*characterTexture_, frameRect_, colour),
                    states);
        return;
    }

    const sf::Vector2u frameSize = bufferSize(frameRect_);
    sf::RenderTexture& effect = ensureBuffer(effectBuffer_, frameSize);
    sf::Sprite source(*characterTexture_, frameRect_);
    sf::RenderStates shaderStates;
    shaderStates.shader = shader_.get();
    effect.clear(sf::Color::Transparent);
    effect.draw(source, shaderStates);
    effect.display();

    sf::RenderTexture& hueBuffer = ensureBuffer(hueBuffer_, frameSize);
    hueShader_->setUniform("screenTex", sf::Shader::CurrentTexture);
    hueShader_->setUniform("hue", hue_);
    sf::Sprite hueSource(effect.getTexture());
    sf::RenderStates hueStates;
    hueStates.shader = hueShader_.get();
    hueBuffer.clear(sf::Color::Transparent);
    hueBuffer.draw(hueSource, hueStates);
    hueBuffer.display();

    const sf::IntRect resultRect(
        {0, 0}, {static_cast<int>(frameSize.x), static_cast<int>(frameSize.y)});
    target.draw(finalSprite(hueBuffer.getTexture(), resultRect, colour),
                states);
}

sf::IntRect CharacterView::fullTextureRect(const sf::Texture& texture) {
    const sf::Vector2u size = texture.getSize();
    return {{0, 0}, {static_cast<int>(size.x), static_cast<int>(size.y)}};
}

sf::Vector2f CharacterView::defaultViewSize(const sf::IntRect& frameRect) {
    return {static_cast<float>(std::max(1, std::abs(frameRect.size.x))),
            static_cast<float>(std::max(1, std::abs(frameRect.size.y)))};
}

sf::Vector2u CharacterView::bufferSize(const sf::IntRect& frameRect) {
    return {
        static_cast<unsigned int>(std::max(1, std::abs(frameRect.size.x))),
        static_cast<unsigned int>(std::max(1, std::abs(frameRect.size.y))),
    };
}

float CharacterView::normaliseHue(float hue) {
    float result = std::fmod(hue, 360.0f);
    if (result < 0.0f) {
        result += 360.0f;
    }
    return result;
}

bool CharacterView::neutralHue(float hue) {
    return hue <= HueEpsilon || std::abs(hue - 360.0f) <= HueEpsilon;
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

void CharacterView::applyShaderPath() {
    shader_.reset();
    shaderError_ = false;
    if (!shaderPath_.empty()) {
        const ludork::engine::actor_impl::ShaderResult result =
            ludork::engine::actor_impl::loadShader(shaderPath_);
        shader_ = result.shader;
        shaderError_ = result.failed;
    }
    hueShader_.reset();
    if (!neutralHue(hue_) && sf::Shader::isAvailable()) {
        hueShader_ = ludork::engine::actor_impl::loadShader(
                         "/Game/Assets/Shaders/Global/Hue.frag")
                         .shader;
    }
}

void CharacterView::applyActorShaderUniforms() const {
    const sf::Vector2u textureSize = characterTexture_->getSize();
    shader_->setUniform("texture", sf::Shader::CurrentTexture);
    shader_->setUniform("time", shaderTime_);
    shader_->setUniform("textureSize",
                        sf::Vector2f{static_cast<float>(textureSize.x),
                                     static_cast<float>(textureSize.y)});
    shader_->setUniform(
        "textureRect", sf::Glsl::Vec4(static_cast<float>(frameRect_.position.x),
                                      static_cast<float>(frameRect_.position.y),
                                      static_cast<float>(frameRect_.size.x),
                                      static_cast<float>(frameRect_.size.y)));
}

sf::RenderTexture& CharacterView::ensureBuffer(
    std::shared_ptr<sf::RenderTexture>& buffer,
    const sf::Vector2u& size) const {
    if (buffer == nullptr || buffer->getSize() != size) {
        buffer = std::make_shared<sf::RenderTexture>(size);
        buffer->setSmooth(false);
    }
    return *buffer;
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
