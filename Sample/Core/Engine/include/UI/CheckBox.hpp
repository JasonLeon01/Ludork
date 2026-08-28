#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <UI/ControlBase.hpp>
#include <UI/FunctionalBase.hpp>
#include <UI/Text.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/System/Vector2.hpp>

#include <functional>
#include <memory>

class Rect;

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API CheckBox : public ControlBase, public FunctionalBase {
public:
    BIND_INIT()
    CheckBox(const sf::Vector2f& size, const sf::Image& windowSkin,
             std::shared_ptr<PlainTextConfig> textConfig, bool checked);
    virtual ~CheckBox();

    CheckBox(const CheckBox&) = delete;
    CheckBox& operator=(const CheckBox&) = delete;
    CheckBox(CheckBox&&) = delete;
    CheckBox& operator=(CheckBox&&) = delete;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD()
    void resize(const sf::Vector2f& size);

    BIND_METHOD(Pure = true)
    bool isChecked() const;

    BIND_METHOD()
    void setChecked(bool checked);

    BIND_METHOD()
    void toggle();

    BIND_METHOD()
    void setWindowSkin(const sf::Image& windowSkin);

    BIND_METHOD()
    void setTextConfig(std::shared_ptr<PlainTextConfig> textConfig);

    BIND_METHOD()
    void setOnCheckedChanged(std::function<void(bool)> callback);

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getLocalBounds() const override;

    BIND_METHOD()
    virtual void update(float deltaTime) override;

    BIND_METHOD()
    virtual void onConfirm(const RuntimeValue::Map& arguments) override;

    BIND_METHOD()
    virtual void onClick(const RuntimeValue::Map& arguments) override;

    BIND_METHOD()
    virtual bool onMouseButtonDown(const RuntimeValue::Map& arguments) override;

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
    static sf::Vector2u integerSize(const sf::Vector2f& size);
    void rebuildMark(std::shared_ptr<PlainTextConfig> textConfig);
    void updateMark();

    sf::Vector2f size_;
    std::unique_ptr<Rect> frame_;
    std::unique_ptr<PlainText> mark_;
    bool checked_ = false;
    bool suppressClick_ = false;
    std::function<void(bool)> checkedChangedCallback_;
};
