#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <UI/ControlBase.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/System/Vector2.hpp>

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API ProgressBar : public ControlBase {
public:
    BIND_INIT()
    ProgressBar(const sf::Vector2f& size, float progress,
                const sf::Color& backgroundColor, const sf::Color& fillColor);
    virtual ~ProgressBar() = default;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD()
    void resize(const sf::Vector2f& size);

    BIND_METHOD(Pure = true)
    float getProgress() const;

    BIND_METHOD()
    void setProgress(float progress);

    BIND_METHOD(Pure = true)
    sf::Color getBackgroundColor() const;

    BIND_METHOD()
    void setBackgroundColor(const sf::Color& color);

    BIND_METHOD(Pure = true)
    sf::Color getFillColor() const;

    BIND_METHOD()
    void setFillColor(const sf::Color& color);

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getLocalBounds() const override;

    void refreshDisplayScale() override;

protected:
    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

private:
    static sf::Vector2f normalizedSize(const sf::Vector2f& size);
    static float normalizedProgress(float progress);
    void updateGeometry();

    sf::Vector2f size_;
    float progress_ = 0.0f;
    sf::RectangleShape background_;
    sf::RectangleShape fill_;
};
