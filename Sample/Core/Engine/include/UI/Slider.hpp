#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <UI/ControlBase.hpp>
#include <UI/FunctionalBase.hpp>

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include <functional>
#include <memory>
#include <utility>

BIND_CLASS(callbacks =
               "getSize,getLocalBounds,update,onClick,onMouseButtonDown,"
               "onMouseMoved,onKeyDown,draw")
class LUDORK_ENGINE_API Slider : public ControlBase, public FunctionalBase {
public:
    BIND_INIT()
    Slider(const sf::Vector2f& size, std::shared_ptr<sf::Texture> lineTexture,
           std::shared_ptr<sf::Texture> handleTexture, int minValue,
           int maxValue, int value);
    virtual ~Slider();

    Slider(const Slider&) = delete;
    Slider& operator=(const Slider&) = delete;
    Slider(Slider&&) = delete;
    Slider& operator=(Slider&&) = delete;

    BIND_METHOD()
    void setVisible(bool visible) override;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD()
    void resize(const sf::Vector2f& size);

    BIND_METHOD(Pure = true)
    int getValue() const;

    BIND_METHOD()
    void setValue(int value);

    BIND_METHOD()
    void setRange(int minValue, int maxValue);

    BIND_METHOD(Pure = true, multiple_returns = true,
                returns = "minimum,maximum")
    std::pair<int, int> getRange() const;

    BIND_METHOD()
    void setValueFromRatio(float ratio);

    BIND_METHOD()
    void setValueFromBoundsPosition(const sf::FloatRect& bounds,
                                    const sf::Vector2f& position);

    BIND_METHOD()
    void adjust(int delta);

    BIND_METHOD(Pure = true)
    int getHandlePosition() const;

    BIND_METHOD()
    void setLineTexture(std::shared_ptr<sf::Texture> texture);

    BIND_METHOD()
    void setHandleTexture(std::shared_ptr<sf::Texture> texture);

    BIND_METHOD()
    void setOnValueChanged(std::function<void(int)> callback);

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getLocalBounds() const override;

    BIND_METHOD()
    virtual void update(float deltaTime) override;

    BIND_METHOD()
    virtual void onClick(const RuntimeValue::Map& arguments) override;

    BIND_METHOD()
    virtual bool onMouseButtonDown(const RuntimeValue::Map& arguments) override;

    BIND_METHOD()
    virtual void onMouseMoved(const RuntimeValue::Map& arguments) override;

    BIND_METHOD()
    virtual void onKeyDown(const RuntimeValue::Map& arguments) override;

    BIND_IGNORE()
    void refreshDisplayScale() override;

protected:
    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

private:
    static sf::Vector2f normalizedSize(const sf::Vector2f& size);
    static std::shared_ptr<sf::Texture> requireTexture(
        std::shared_ptr<sf::Texture> texture);
    int clampValue(int value) const;
    float valueRatio() const;
    float handleWidth() const;
    float handleOffset() const;
    void updateGeometry();
    void updatePointerDrag();

    sf::Vector2f size_;
    std::shared_ptr<sf::Texture> lineTexture_;
    std::shared_ptr<sf::Texture> handleTexture_;
    std::unique_ptr<sf::Sprite> line_;
    std::unique_ptr<sf::Sprite> handle_;
    int minValue_ = 0;
    int maxValue_ = 100;
    int value_ = 0;
    bool mouseDragging_ = false;
    bool touchDragging_ = false;
    bool suppressClick_ = false;
    std::function<void(int)> valueChangedCallback_;
};
