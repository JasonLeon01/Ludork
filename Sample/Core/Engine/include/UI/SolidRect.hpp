#pragma once

#include <CoreMinimal.hpp>

#include <UI/ControlBase.hpp>

BIND_CLASS(callbacks = true)
class SolidRect : public ControlBase {
public:
    BIND_INIT()
    explicit SolidRect(const sf::Vector2f& size,
                       const sf::Color& fillColor = sf::Color::White,
                       const sf::Color& outlineColor = sf::Color::Transparent,
                       float outlineThickness = 0.0f);
    virtual ~SolidRect() = default;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD()
    void setSize(const sf::Vector2f& size);

    BIND_METHOD(Pure = true)
    sf::Color getFillColor() const;

    BIND_METHOD()
    void setFillColor(const sf::Color& color);

    BIND_METHOD(Pure = true)
    sf::Color getOutlineColor() const;

    BIND_METHOD()
    void setOutlineColor(const sf::Color& color);

    BIND_METHOD(Pure = true)
    float getOutlineThickness() const;

    BIND_METHOD()
    void setOutlineThickness(float thickness);

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getLocalBounds() const override;

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getGlobalBounds() const;

    void refreshDisplayScale() override;

protected:
    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

    void _refreshPresentationColour() override;

private:
    void applyColours();

    sf::Vector2f size_;
    sf::RectangleShape shape_;
    float outlineThickness_ = 0.0f;
    sf::Color fillColor_ = sf::Color::White;
    sf::Color outlineColor_ = sf::Color::Transparent;
};
