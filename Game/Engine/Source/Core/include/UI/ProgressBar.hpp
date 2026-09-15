#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>
#include <UI/ControlBase.hpp>

#include <array>
#include <memory>
#include <optional>

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API ProgressBar : public ControlBase {
public:
    LUDORK_CAST_DERIVED(ProgressBar, ControlBase)

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
    std::shared_ptr<sf::Texture> getBackgroundTexture() const;

    BIND_METHOD()
    void setBackgroundTexture(std::shared_ptr<sf::Texture> texture);

    BIND_METHOD(Pure = true)
    std::shared_ptr<sf::Texture> getFillTexture() const;

    BIND_METHOD()
    void setFillTexture(std::shared_ptr<sf::Texture> texture);

    BIND_METHOD(Pure = true)
    std::optional<sf::IntRect> getBackgroundTextureRect() const;

    BIND_METHOD()
    void setBackgroundTextureRect(std::optional<sf::IntRect> rect);

    BIND_METHOD(Pure = true)
    std::optional<sf::IntRect> getFillTextureRect() const;

    BIND_METHOD()
    void setFillTextureRect(std::optional<sf::IntRect> rect);

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getLocalBounds() const override;

    void refreshDisplayScale() override;

protected:
    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

    void _refreshPresentationColour() override;

private:
    static sf::Vector2f normalizedSize(const sf::Vector2f& size);
    static float normalizedProgress(float progress);
    void updateGeometry();
    void applyColours();

    sf::Vector2f size_;
    float progress_ = 0.0f;
    std::array<sf::Vertex, 4> background_;
    std::array<sf::Vertex, 4> fill_;
    std::shared_ptr<sf::Texture> backgroundTexture_;
    std::shared_ptr<sf::Texture> fillTexture_;
    std::optional<sf::IntRect> backgroundTextureRect_;
    std::optional<sf::IntRect> fillTextureRect_;
    sf::Color backgroundColor_;
    sf::Color fillColor_;
};
