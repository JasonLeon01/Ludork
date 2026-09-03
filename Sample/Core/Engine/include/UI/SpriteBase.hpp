#pragma once

#include <BindAnnotations.hpp>
#include <UI/ControlBase.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <memory>
#include <optional>

BIND_CLASS(callbacks = true)
class SpriteBase : public ControlBase {
public:
    BIND_INIT()
    explicit SpriteBase(std::shared_ptr<sf::Texture> texture,
                        std::optional<sf::IntRect> rect = std::nullopt);
    virtual ~SpriteBase() = default;

    BIND_METHOD()
    void setTexture(std::shared_ptr<sf::Texture> texture,
                    bool resetRect = false);

    BIND_METHOD(Pure = true)
    const sf::Texture& getTexture() const;

    BIND_METHOD()
    void setTextureRect(const sf::IntRect& rect);

    BIND_METHOD(Pure = true)
    sf::IntRect getTextureRect() const;

    BIND_METHOD()
    void setColour(const sf::Color& colour);

    BIND_METHOD(Pure = true)
    sf::Color getColour() const;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getLocalBounds() const override;

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getGlobalBounds() const;

    BIND_METHOD(Pure = true)
    virtual sf::RenderStates getRenderStates() const override;

protected:
    void setPremultipliedTexture(bool premultiplied);

    sf::Color presentedColour() const;

    void _refreshPresentationColour() override;

    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

    BIND_METHOD(metadata = false)
    virtual void _applyRenderStates(sf::RenderStates& states) const override;

    BIND_METHOD(metadata = false)
    virtual sf::Transform _getRenderTransform() const override;

private:
    void applyColour();

    std::shared_ptr<sf::Texture> texture_;
    std::unique_ptr<sf::Sprite> sprite_;
    sf::RenderStates renderStates_;
    sf::Color colour_ = sf::Color::White;
    bool premultipliedTexture_ = false;
};
