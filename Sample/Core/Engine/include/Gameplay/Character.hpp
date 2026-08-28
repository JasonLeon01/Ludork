#pragma once

#include <BindAnnotations.hpp>
#include <Gameplay/Actor.hpp>

#include <memory>
#include <optional>
#include <string>

BIND_INVALID_VARS(vars = "defaultRect")
BIND_CLASS(callbacks = true)
class Character : public Actor {
public:
    BIND_INIT()
    explicit Character(std::shared_ptr<sf::Texture> texture,
                       std::string tag = "");

    ~Character() override = default;

    BIND_PROPERTY(metadata_type = "Direction")
    int direction = 0;

    BIND_PROPERTY()
    bool directionFix = false;

    BIND_PROPERTY()
    bool animateWithoutMoving = false;

    BIND_METHOD(outpins(default = nil), defaults = {nil, false})
    void setSpriteTexture(std::shared_ptr<sf::Texture> texture,
                          bool resetRect = false) override;

    BIND_METHOD(outpins(default = nil), defaults = {nil, false})
    void setTexture(std::shared_ptr<sf::Texture> texture,
                    bool resetRect = false) override;

    BIND_METHOD(outpins(default = nil))
    void setTextureRect(const sf::IntRect& rectangle) override;

    BIND_METHOD(outpins(success = true, fail = false))
    bool MapMove(const sf::Vector2i& offset) override;

    BIND_METHOD()
    void update(float deltaTime) override;

    BIND_METHOD(defaults = {nil, nil, nil})
    static RuntimeValue GenActor(
        const RuntimeIdentityPtr& actorModel,
        const RuntimeIdentityPtr& texture = nullptr,
        const RuntimeValue& textureRect = {},
        const std::optional<std::string>& tag = std::nullopt);

protected:
    BIND_METHOD(metadata = false)
    void _animate(float deltaTime) override;

    BIND_METHOD(metadata = false)
    virtual void _applyDirection(float x, float y);

private:
    static sf::Vector2i rectSize(const std::shared_ptr<sf::Texture>& texture);

    sf::Vector2i rectSize_;
    int frameX_ = 0;
    int frameY_ = 0;
    float frameTimer_ = 0.0f;
};
