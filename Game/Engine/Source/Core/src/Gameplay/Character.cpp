#include <Gameplay/Character.hpp>

#include <Runtime/RuntimeReflection.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

RuntimeValue optionalStringValue(const std::optional<std::string>& value) {
    return value.has_value() ? RuntimeValue(*value) : RuntimeValue();
}
}  // namespace

Character::Character(std::shared_ptr<sf::Texture> texture, std::string actorTag)
    : Actor(texture, sf::IntRect({0, 0}, Character::rectSize(texture)),
            std::move(actorTag)),
      rectSize_(Character::rectSize(texture)) {}

void Character::setSpriteTexture(std::shared_ptr<sf::Texture> texture,
                                 bool resetRect) {
    const int column = rectSize_.x == 0 ? 0 : frameX_ / rectSize_.x;
    const int row = rectSize_.y == 0 ? 0 : frameY_ / rectSize_.y;
    Actor::setSpriteTexture(texture, resetRect);
    if (!resetRect) {
        return;
    }
    rectSize_ = rectSize(texture);
    setTextureRect(
        sf::IntRect({column * rectSize_.x, row * rectSize_.y}, rectSize_));
}

void Character::setTexture(std::shared_ptr<sf::Texture> texture,
                           bool resetRect) {
    Actor::setTexture(std::move(texture), resetRect);
}

void Character::setTextureRect(const sf::IntRect& rectangle) {
    rectSize_ = rectangle.size;
    Actor::setTextureRect(rectangle);
}

bool Character::MapMove(const sf::Vector2i& offset) {
    if (!getMoveEnabled()) {
        return false;
    }
    const bool result = Actor::MapMove(offset);
    if (!result) {
        _applyDirection(static_cast<float>(offset.x),
                        static_cast<float>(offset.y));
    }
    return result;
}

void Character::update(float deltaTime) {
    if (!directionFix) {
        const std::optional<sf::Vector2f> velocity = getVelocity();
        if (velocity.has_value()) {
            _applyDirection(velocity->x, velocity->y);
        }
        frameY_ = direction * rectSize_.y;
    }
    Actor::update(deltaTime);
}

RuntimeValue Character::GenActor(const RuntimeIdentityPtr& actorModel,
                                 const RuntimeIdentityPtr& texture,
                                 const RuntimeValue& textureRect,
                                 const std::optional<std::string>& tag) {
    static_cast<void>(textureRect);
    RuntimeValue result = runtimeReflection().construct(
        ludork::runtime::reference::intern(RuntimeValue(actorModel)),
        {RuntimeValue(texture), optionalStringValue(tag)});
    if (result.isNil()) {
        throw std::runtime_error(
            "Character runtime construction returned no instance");
    }
    return result;
}

void Character::_animate(float deltaTime) {
    const std::shared_ptr<sf::Texture> texture = getSpriteTexture();
    if (!texture) {
        return;
    }
    if (isMoving() || animateWithoutMoving) {
        frameTimer_ += deltaTime;
        if (switchInterval > 0.0f) {
            while (frameTimer_ >= switchInterval) {
                frameTimer_ -= switchInterval;
                const unsigned int textureWidth = texture->getSize().x;
                if (textureWidth != 0) {
                    frameX_ = (frameX_ + rectSize_.x) %
                              static_cast<int>(textureWidth);
                }
            }
        }
    } else {
        frameX_ = 0;
    }
    const sf::IntRect nextRect({frameX_, frameY_}, rectSize_);
    if (nextRect != getTextureRect()) {
        setTextureRect(nextRect);
    }
}

void Character::_applyDirection(float x, float y) {
    if (std::abs(x) > std::abs(y)) {
        direction = x > 0.0f ? 2 : 1;
    } else {
        direction = y > 0.0f ? 0 : 3;
    }
}

sf::Vector2i Character::rectSize(const std::shared_ptr<sf::Texture>& texture) {
    if (!texture) {
        throw std::invalid_argument("Character requires a texture");
    }
    const sf::Vector2u size = texture->getSize();
    return {static_cast<int>(size.x / 4), static_cast<int>(size.y / 4)};
}
