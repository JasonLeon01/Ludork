#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Particles/ParticleBase.hpp>
#include <UI/Text.hpp>

#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Transformable.hpp>

#include <functional>
#include <memory>
#include <string>

class LUDORK_ENGINE_API ParticleSystem;

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API TextParticle : public ParticleBase,
                                       public sf::Drawable,
                                       public sf::Transformable {
public:
    TextParticle() = delete;

    BIND_INIT()
    TextParticle(std::shared_ptr<ParticleSystem> parent,
                 std::function<void(float, float, ParticleBase*)> moveFunction,
                 float countTime, const std::string& text,
                 std::shared_ptr<PlainTextConfig> config,
                 bool logicalCoordinates = false);

    BIND_METHOD(Pure = true)
    std::shared_ptr<PlainTextConfig> getConfig() const;

    BIND_METHOD()
    void setString(const std::string& text);

    BIND_METHOD(Pure = true)
    std::string getString() const;

    BIND_METHOD()
    void setColour(const sf::Color& colour);

    BIND_METHOD(Pure = true)
    sf::Color getColour() const;

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getLocalBounds() const;

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getGlobalBounds() const;

protected:
    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

private:
    std::shared_ptr<PlainText> text_;
    bool logicalCoordinates_ = false;
};
