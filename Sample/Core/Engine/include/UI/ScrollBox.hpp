#pragma once

#include <EngineRuntimeApi.hpp>

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <UI/Canvas.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include <array>
#include <memory>
#include <optional>

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API ScrollBox : public Canvas {
public:
    BIND_INIT()
    ScrollBox(const sf::Vector2f& size, const sf::Image& windowSkin);
    virtual ~ScrollBox() = default;

    BIND_METHOD(Pure = true)
    sf::Vector2f getSize() const override;

    BIND_METHOD()
    void resize(const sf::Vector2f& size);

    BIND_METHOD()
    void setWindowSkin(const sf::Image& windowSkin);

    BIND_METHOD(Pure = true)
    sf::Vector2f getScrollOffset() const;

    BIND_METHOD()
    void setScrollOffset(const sf::Vector2f& offset);

    BIND_METHOD(Pure = true)
    sf::Vector2f getMaxScrollOffset() const;

    BIND_METHOD(Pure = true)
    bool getScrollingEnabled() const;

    BIND_METHOD()
    void setScrollingEnabled(bool enabled);

    BIND_METHOD()
    void scrollDescendantIntoView(
        const std::shared_ptr<ControlBase>& descendant);

    BIND_METHOD()
    void update(float deltaTime) override;

    BIND_METHOD()
    void render() override;

    BIND_METHOD()
    void onMouseMoved(const RuntimeValue::Map& arguments) override;

    BIND_METHOD()
    void onMouseWheelScrolled(const RuntimeValue::Map& arguments) override;

protected:
    bool acceptsTouchCapture() const override;
    void onTouchCaptureBegan(const sf::Vector2f& position) override;
    void onPointerInteractionReset() override;
    std::optional<sf::FloatRect> _getAbsoluteChildInteractionClipBounds()
        const override;

public:
    sf::FloatRect getAbsoluteBounds() const override;

private:
    enum class Indicator {
        Up,
        Down,
        Left,
        Right,
        Count,
    };

    static constexpr float WheelStep = 32.0f;
    static constexpr float WheelResponse = 18.0f;
    static constexpr float WheelEpsilon = 0.01f;

    static sf::Vector2u roundedSize(const sf::Vector2f& size);
    static std::size_t indicatorIndex(Indicator indicator);
    sf::FloatRect aggregateContentBounds() const;
    void clampOffsets();
    void setScrollTargetOffset(const sf::Vector2f& offset);
    void updateWheelScroll(float deltaTime);
    void applyView();
    void updateTouchArbitration();
    void applyTouchScroll(const sf::Vector2f& position);
    std::shared_ptr<ControlBase> findCapturedDescendant(
        const std::shared_ptr<ControlBase>& root) const;
    void rebuildIndicators(const sf::Image& windowSkin);
    void drawIndicators();
    bool isDescendant(const std::shared_ptr<ControlBase>& control) const;

    sf::Vector2f scrollOffset_;
    bool scrollingEnabled_ = true;
    std::optional<sf::Vector2f> scrollTargetOffset_;
    sf::Vector2f touchStartPosition_;
    sf::Vector2f touchStartOffset_;
    std::weak_ptr<ControlBase> touchArbitratedChild_;
    bool touchArbitrationActive_ = false;
    bool touchScrollOwned_ = false;
    bool touchChildOwned_ = false;
    std::array<std::unique_ptr<sf::Texture>,
               static_cast<std::size_t>(Indicator::Count)>
        indicatorTextures_;
    std::array<std::unique_ptr<sf::Sprite>,
               static_cast<std::size_t>(Indicator::Count)>
        indicatorSprites_;
};
