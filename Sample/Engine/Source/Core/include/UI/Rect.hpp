#pragma once

#include <CoreMinimal.hpp>

#include <Curve.hpp>
#include <EngineRuntimeApi.hpp>
#include <Graphics/RectBase.hpp>
#include <UI/SpriteBase.hpp>

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API Rect : public SpriteBase {
public:
    BIND_INIT()
    Rect(const sf::IntRect& rect, const sf::Image& windowSkin,
         std::optional<std::string> opacityCurveKey = std::nullopt);
    virtual ~Rect() = default;

    BIND_METHOD()
    void setOpacityMultiplier(float multiplier);

    BIND_METHOD()
    void resize(const sf::Vector2f& size);

    BIND_METHOD()
    void setWindowSkin(const sf::Image& windowSkin);

    BIND_METHOD(Pure = true, lua_return_type = "sf::Vector2u")
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD()
    virtual void update(float deltaTime);

    void refreshDisplayScale() override;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string SelectionRectOpacityCurveKey;

    static void clearOpacityCurveCache() noexcept;

private:
    static std::shared_ptr<sf::Texture> placeholderTexture();
    static std::shared_ptr<Curve> resolveOpacityCurve(const std::string& key);

    void bindCanvasTexture();
    void initialiseUi();
    std::unique_ptr<sf::Texture> textureFromArea(const sf::IntRect& area) const;
    std::shared_ptr<Curve> opacityCurve() const;
    void updateFallbackOpacity(float deltaTime);
    void applyOpacity();

    static std::unordered_map<std::string, std::shared_ptr<Curve>>
        opacityCurves_;

    sf::Vector2f size_;
    std::shared_ptr<sf::RenderTexture> canvas_;
    sf::Image windowSkin_;
    RectBase rectImpl_;
    std::unique_ptr<sf::RenderTexture> windowEdge_;
    std::unique_ptr<sf::Texture> windowBack_;
    std::unique_ptr<sf::Sprite> windowEdgeSprite_;
    std::unique_ptr<sf::Sprite> windowBackSprite_;
    std::vector<std::unique_ptr<sf::Texture>> cachedCornerTextures_;
    std::vector<std::unique_ptr<sf::Texture>> cachedEdgeTextures_;
    std::string opacityCurveKey_;
    float opacityTime_ = 0.0f;
    float opacity_ = 255.0f;
    float opacityMultiplier_ = 1.0f;
    bool fading_ = true;
};
