#include <Gameplay/Actor.hpp>

#include <ConcurrentResourceCache.hpp>
#include <Utils/ShaderLoader.hpp>

#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

std::unique_ptr<sf::Texture>& blankTextureStorage() {
    static std::unique_ptr<sf::Texture> texture;
    return texture;
}

ludork::core::ConcurrentResourceCache<sf::Shader, true>& actorShaderCache() {
    static ludork::core::ConcurrentResourceCache<sf::Shader, true> cache;
    return cache;
}

const sf::Texture& blankTexture() {
    std::unique_ptr<sf::Texture>& texture = blankTextureStorage();
    if (texture == nullptr) {
        texture = std::make_unique<sf::Texture>();
    }
    return *texture;
}

}  // namespace

const sf::Texture& Actor::textureOrBlank(
    const std::shared_ptr<sf::Texture>& texture) {
    return texture ? *texture : blankTexture();
}

void Actor::ensureShaderLoaded() const {
    if (loadedShaderPath_ == shaderPath) {
        return;
    }
    loadedShaderPath_ = shaderPath;
    shader_.reset();
    shaderError_ = false;
    if (shaderPath.empty()) {
        return;
    }
    try {
        shader_ = actorShaderCache().getOrLoad(shaderPath, [this]() {
            ShaderLoadResult result =
                ShaderLoader::load(shaderPath, sf::Shader::Type::Fragment);
            if (!result) {
                throw std::runtime_error(result.error);
            }
            return std::move(result.shader);
        });
    } catch (const std::exception& error) {
        shaderError_ = true;
        std::cerr << error.what() << '\n';
    }
}

void Actor::setShaderPath(const std::string& shaderPath) {
    this->shaderPath = shaderPath;
    loadedShaderPath_.clear();
    ensureShaderLoaded();
}

const std::string& Actor::getShaderPath() const {
    return shaderPath;
}

std::shared_ptr<sf::Shader> Actor::getShader() const {
    ensureShaderLoaded();
    return shader_;
}

bool Actor::hasShaderError() const {
    ensureShaderLoaded();
    return shaderError_;
}

bool Actor::getVisible() const {
    return visible_;
}

void Actor::setVisible(bool visible, bool applyToChildren) {
    visible_ = visible;
    if (applyToChildren) {
        for (const std::shared_ptr<Actor>& child : children_) {
            child->setVisible(visible, true);
        }
    }
}

bool Actor::getAnimatable() const {
    return animatable;
}

void Actor::setAnimatable(bool animate, bool applyToChildren) {
    animatable = animate;
    if (applyToChildren) {
        for (const std::shared_ptr<Actor>& child : children_) {
            child->setAnimatable(animate, true);
        }
    }
}

std::shared_ptr<sf::Texture> Actor::getSpriteTexture() const {
    return spriteTexture_;
}

std::shared_ptr<sf::Texture> Actor::getTexture() const {
    return texture_;
}

void Actor::setSpriteTexture(std::shared_ptr<sf::Texture> texture,
                             bool resetRect) {
    if (!texture) {
        throw std::invalid_argument("Actor texture must not be null");
    }
    spriteTexture_ = std::move(texture);
    sf::Sprite::setTexture(*spriteTexture_, resetRect);
    syncMapCache();
}

void Actor::setTexture(std::shared_ptr<sf::Texture> texture, bool resetRect) {
    texture_ = texture;
    setSpriteTexture(std::move(texture), resetRect);
}

void Actor::setTextureRect(const sf::IntRect& rectangle) {
    const sf::Vector2i previousSize = sf::Sprite::getTextureRect().size;
    sf::Sprite::setTextureRect(rectangle);
    if (previousSize != rectangle.size) {
        syncMapCache();
    }
}

sf::IntRect Actor::getTextureRect() const {
    return sf::Sprite::getTextureRect();
}

Material Actor::getMaterial() const {
    return material;
}

void Actor::setMaterial(const std::optional<Material>& material) {
    this->material = material.value_or(Material{});
}

float Actor::getLightBlock() const {
    return material.lightBlock;
}

void Actor::setLightBlock(float lightBlock) {
    material.lightBlock = lightBlock;
}

bool Actor::getMirror() const {
    return material.mirror;
}

void Actor::setMirror(bool mirror) {
    material.mirror = mirror;
}

float Actor::getReflectionStrength() const {
    return material.reflectionStrength;
}

void Actor::setReflectionStrength(float reflectionStrength) {
    material.reflectionStrength = reflectionStrength;
}

float Actor::getOpacity() const {
    return material.opacity;
}

void Actor::setOpacity(float opacity) {
    material.opacity = opacity;
}

bool Actor::getIgnoreLighting() const {
    return material.ignoreLighting;
}

void Actor::setIgnoreLighting(bool ignoreLighting) {
    material.ignoreLighting = ignoreLighting;
}

void Actor::_animate(float deltaTime) {
    if (!texture_) {
        return;
    }
    switchTimer_ += deltaTime;
    if (switchTimer_ < switchInterval) {
        return;
    }
    switchTimer_ = 0.0f;
    const sf::IntRect currentRect = getTextureRect();
    const unsigned int width = texture_->getSize().x;
    if (width == 0) {
        return;
    }
    const sf::IntRect nextRect({(currentRect.position.x + currentRect.size.x) %
                                    static_cast<int>(width),
                                currentRect.position.y},
                               currentRect.size);
    if (nextRect != currentRect) {
        setTextureRect(nextRect);
    }
}

void shutdownActorResources() noexcept {
    actorShaderCache().clear();
    blankTextureStorage().reset();
}

sf::Color Actor::getLightColour() const {
    return lightComp ? lightComp->lightColour : sf::Color::White;
}

void Actor::setLightColour(const sf::Color& colour) {
    if (!lightComp) {
        lightComp = std::make_shared<LightComponent>();
    }
    lightComp->lightColour = colour;
}

float Actor::getLightRadius() const {
    return lightComp ? lightComp->lightRadius : 16.0f;
}

void Actor::setLightRadius(float radius) {
    if (!lightComp) {
        lightComp = std::make_shared<LightComponent>();
    }
    lightComp->lightRadius = radius;
}
