#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>
#include <Input/InputAction.hpp>
#include <UI/ControlBase.hpp>
#include <UI/FunctionalBase.hpp>
#include <UI/Text.hpp>

class PlainText;
class Rect;
class SolidRect;

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API TabView : public ControlBase, public FunctionalBase {
public:
    BIND_INIT(defaults = {0})
    TabView(const sf::Vector2f& size, const sf::Image& windowSkin,
            std::shared_ptr<PlainTextConfig> textConfig,
            std::vector<std::string> items, int selectedIndex = 0);
    virtual ~TabView();

    TabView(const TabView&) = delete;
    TabView& operator=(const TabView&) = delete;
    TabView(TabView&&) = delete;
    TabView& operator=(TabView&&) = delete;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD()
    void resize(const sf::Vector2f& size);

    BIND_METHOD()
    void setWindowSkin(const sf::Image& windowSkin);

    BIND_METHOD()
    void setTextConfig(std::shared_ptr<PlainTextConfig> textConfig);

    BIND_METHOD(Pure = true)
    std::vector<std::string> getItems() const;

    BIND_METHOD()
    void setItems(const std::vector<std::string>& items,
                  std::optional<std::function<void(int)>> callback);

    BIND_METHOD(Pure = true)
    int getSelectedIndex() const;

    BIND_METHOD()
    void setSelectedIndex(int index);

    BIND_METHOD(Pure = true)
    std::string getSelectedItem() const;

    BIND_METHOD()
    bool selectPrevious();

    BIND_METHOD()
    bool selectNext();

    BIND_METHOD()
    bool handleNavigationInput();

    BIND_METHOD()
    void setCursorSound(const std::string& filename);

    BIND_METHOD(Pure = true)
    const std::string& getCursorSound() const;

    BIND_METHOD()
    void setKeyHint(const RuntimeValue::Map& leftHint,
                    const RuntimeValue::Map& rightHint);

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

    void refreshDisplayScale() override;

    void releaseRuntimeCallbacks() noexcept override;

protected:
    void _refreshPresentationColour() override;

    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

    bool acceptsTouchCapture() const override;

private:
    struct KeyHint {
        std::optional<std::string> keyboard;
        std::optional<std::string> handle;
    };

    static sf::Vector2f normalizedSize(const sf::Vector2f& size);
    static int clampedIndex(int index, std::size_t count);
    static KeyHint parseKeyHint(const RuntimeValue::Map& values,
                                const std::string& source);
    static bool anyJoystickConnected();
    static bool keyboardHintsAvailableWithoutJoystick();

    bool setSelectedIndexInternal(int index, bool playSound);
    bool selectPointerPosition(const sf::Vector2f& screenPosition);
    void selectMouseHover(const RuntimeValue::Map& arguments);
    std::optional<int> tabIndexAt(const sf::Vector2f& localPosition) const;
    sf::Vector2f toLocalPosition(const sf::Vector2f& screenPosition) const;
    void rebuildVisuals();
    void rebuildLabels();
    void rebuildHintVisuals();
    void layoutVisuals();
    void layoutLabel(PlainText& label, int index) const;
    void layoutHint(PlainText& text, SolidRect& background, bool left) const;
    void updateSelectionVisual();
    void updateHintVisibility();
    void applyPresentationColour();
    const std::optional<std::string>& visibleHint(const KeyHint& hint) const;

    sf::Vector2f size_;
    sf::Image windowSkin_;
    std::shared_ptr<PlainTextConfig> textConfig_;
    std::vector<std::string> items_;
    int selectedIndex_ = 0;
    KeyHint leftHint_;
    KeyHint rightHint_;
    std::function<void(int)> selectedIndexChangedCallback_;
    std::string cursorSound_;
    std::unique_ptr<Rect> selectionRect_;
    std::vector<std::unique_ptr<PlainText>> labels_;
    std::unique_ptr<SolidRect> leftHintBackground_;
    std::unique_ptr<SolidRect> rightHintBackground_;
    std::unique_ptr<PlainText> leftHintText_;
    std::unique_ptr<PlainText> rightHintText_;
    bool suppressNextClick_ = false;
};
