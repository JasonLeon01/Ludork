#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

BIND_CLASS()
class LUDORK_ENGINE_API SectorShape : public sf::Drawable,
                                      public sf::Transformable {
public:
    BIND_INIT()
    explicit SectorShape(float radius = 0.0f,
                         const sf::Color& fillColour = sf::Color::White);
    ~SectorShape() override = default;

    BIND_METHOD(Pure = true)
    float getRadius() const;

    BIND_METHOD()
    void setRadius(float radius);

    BIND_METHOD(Pure = true)
    sf::Angle getAngle() const;

    BIND_METHOD()
    void setAngle(sf::Angle angle);

    BIND_METHOD(Pure = true)
    sf::Color getFillColour() const;

    BIND_METHOD()
    void setFillColour(const sf::Color& colour);

    BIND_METHOD(Pure = true)
    sf::FloatRect getLocalBounds() const;

    BIND_METHOD(Pure = true)
    sf::FloatRect getGlobalBounds() const;

protected:
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    static float normalizedRadius(float radius);
    static sf::Angle normalizedAngle(sf::Angle angle);

    void rebuild();

    float radius_ = 0.0f;
    sf::Angle angle_;
    sf::Color fillColour_ = sf::Color::White;
    sf::VertexArray vertices_{sf::PrimitiveType::TriangleFan};
};
