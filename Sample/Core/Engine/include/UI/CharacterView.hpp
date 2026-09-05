#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>
#include <UI/FunctionalUI.hpp>

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API CharacterView : public FunctionalImage {
public:
    BIND_INIT(defaults = {nil, nil, nil, true, 0.2, "", 0.0})
    explicit CharacterView(std::shared_ptr<sf::Texture> texture,
                           std::optional<sf::IntRect> frameRect = std::nullopt,
                           std::optional<sf::Vector2f> size = std::nullopt,
                           bool animatable = true, float switchInterval = 0.2f,
                           std::string shaderPath = "", float hue = 0.0f);

    ~CharacterView() override = default;

    BIND_METHOD(defaults = {nil, nil, nil, nil, nil, "", 0.0})
    void setCharacter(std::shared_ptr<sf::Texture> texture,
                      const sf::IntRect& frameRect,
                      const sf::Vector2f& characterScale, bool animatable,
                      float switchInterval, const std::string& shaderPath = "",
                      float hue = 0.0f);

    BIND_METHOD()
    void setCharacterTexture(std::shared_ptr<sf::Texture> texture);

    BIND_METHOD(Pure = true)
    sf::Vector2f getCharacterScale() const;

    BIND_METHOD()
    void setCharacterScale(const sf::Vector2f& scale);

    BIND_METHOD(Pure = true)
    bool getAnimatable() const;

    BIND_METHOD()
    void setAnimatable(bool animatable);

    BIND_METHOD(Pure = true)
    float getSwitchInterval() const;

    BIND_METHOD()
    void setSwitchInterval(float switchInterval);

    BIND_METHOD(Pure = true)
    float getSwitchTimer() const;

    BIND_METHOD(Pure = true)
    sf::IntRect getFrameRect() const;

    BIND_METHOD()
    void setFrameRect(const sf::IntRect& frameRect);

    BIND_METHOD()
    void resetFrameRectToTexture();

    BIND_METHOD(Pure = true)
    const std::string& getShaderPath() const;

    BIND_METHOD()
    void setShaderPath(const std::string& shaderPath);

    BIND_METHOD(Pure = true)
    float getHue() const;

    BIND_METHOD()
    void setHue(float hue);

    BIND_METHOD()
    void resetAnimation();

    BIND_METHOD(Pure = true)
    sf::Vector2f getSize() const override;

    BIND_METHOD(Pure = true)
    sf::FloatRect getLocalBounds() const override;

    BIND_METHOD(Pure = true)
    sf::FloatRect getGlobalBounds() const override;

    BIND_METHOD()
    void resize(const sf::Vector2f& size);

    BIND_METHOD(callback = false)
    void update(float deltaTime) override;

protected:
    BIND_METHOD()
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    static sf::IntRect fullTextureRect(const sf::Texture& texture);
    static sf::Vector2f defaultViewSize(const sf::IntRect& frameRect);
    static sf::Vector2u bufferSize(const sf::IntRect& frameRect);
    static float normaliseHue(float hue);
    static bool neutralHue(float hue);
    static sf::Vector2f nonZeroScale(const sf::Vector2f& scale);

    void applyShaderPath();
    void applyActorShaderUniforms() const;
    sf::RenderTexture& ensureBuffer(std::shared_ptr<sf::RenderTexture>& buffer,
                                    const sf::Vector2u& size) const;
    sf::Sprite finalSprite(const sf::Texture& texture,
                           const sf::IntRect& textureRect,
                           const sf::Color& colour) const;

    std::shared_ptr<sf::Texture> characterTexture_;
    sf::IntRect initialFrameRect_;
    sf::IntRect frameRect_;
    sf::Vector2f characterScale_{1.0f, 1.0f};
    sf::Vector2f size_;
    bool animatable_ = true;
    float switchInterval_ = 0.2f;
    float switchTimer_ = 0.0f;
    float shaderTime_ = 0.0f;
    std::string shaderPath_;
    std::shared_ptr<sf::Shader> shader_;
    std::shared_ptr<sf::Shader> hueShader_;
    bool shaderError_ = false;
    float hue_ = 0.0f;
    mutable std::shared_ptr<sf::RenderTexture> effectBuffer_;
    mutable std::shared_ptr<sf::RenderTexture> hueBuffer_;
};
