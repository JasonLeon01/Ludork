#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <UI/ControlBase.hpp>
#include <UI/FunctionalBase.hpp>
#include <UI/Text.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Vector2.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class PlainText;
class Rect;
class Window;

BIND_CLASS(callbacks =
               "getSize,getLocalBounds,update,onConfirm,onCancel,onClick,"
               "onMouseButtonDown,onMouseMoved,onMouseWheelScrolled,onKeyDown,"
               "draw")
class LUDORK_ENGINE_API DropBox : public ControlBase, public FunctionalBase {
public:
    BIND_INIT(defaults = {{}, 0, false})
    DropBox(const sf::Vector2f& collapsedSize, const sf::Image& windowSkin,
            std::shared_ptr<PlainTextConfig> textConfig,
            std::vector<std::string> items = {}, int selectedIndex = 0,
            bool repeated = false);
    virtual ~DropBox();

    DropBox(const DropBox&) = delete;
    DropBox& operator=(const DropBox&) = delete;
    DropBox(DropBox&&) = delete;
    DropBox& operator=(DropBox&&) = delete;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getLocalBounds() const override;

    BIND_METHOD()
    void resize(const sf::Vector2f& size);

    BIND_METHOD(defaults = {false})
    void setWindowSkin(const sf::Image& windowSkin, bool repeated = false);

    BIND_METHOD()
    void setTextConfig(std::shared_ptr<PlainTextConfig> textConfig);

    BIND_METHOD(Pure = true)
    std::vector<std::string> getItems() const;

    BIND_METHOD()
    void setItems(const std::vector<std::string>& items);

    BIND_METHOD(Pure = true)
    int getSelectedIndex() const;

    BIND_METHOD()
    void setSelectedIndex(int index);

    BIND_METHOD(Pure = true)
    std::string getSelectedItem() const;

    BIND_METHOD(Pure = true)
    bool isExpanded() const;

    BIND_METHOD()
    void setExpanded(bool expanded);

    BIND_METHOD()
    void open();

    BIND_METHOD()
    void cancel();

    BIND_METHOD()
    void setOnSelectedIndexChanged(std::function<void(int)> callback);

    BIND_METHOD()
    void setOnSelectionConfirmed(std::function<void(int)> callback);

    BIND_METHOD()
    void setOnExpandedChanged(std::function<void(bool)> callback);

    BIND_METHOD()
    void setOpenSound(const std::string& filename);

    BIND_METHOD(Pure = true)
    const std::string& getOpenSound() const;

    BIND_METHOD()
    void setCursorSound(const std::string& filename);

    BIND_METHOD(Pure = true)
    const std::string& getCursorSound() const;

    BIND_METHOD()
    void setSelectSound(const std::string& filename);

    BIND_METHOD(Pure = true)
    const std::string& getSelectSound() const;

    BIND_METHOD()
    void setCancelSound(const std::string& filename);

    BIND_METHOD(Pure = true)
    const std::string& getCancelSound() const;

    BIND_METHOD()
    virtual void update(float deltaTime) override;

    BIND_METHOD()
    virtual void onConfirm(const RuntimeValue::Map& arguments) override;

    BIND_METHOD()
    virtual void onCancel(const RuntimeValue::Map& arguments) override;

    BIND_METHOD()
    virtual void onClick(const RuntimeValue::Map& arguments) override;

    BIND_METHOD()
    virtual bool onMouseButtonDown(const RuntimeValue::Map& arguments) override;

    BIND_METHOD()
    virtual void onMouseMoved(const RuntimeValue::Map& arguments) override;

    BIND_METHOD()
    virtual void onMouseWheelScrolled(
        const RuntimeValue::Map& arguments) override;

    BIND_METHOD()
    virtual void onKeyDown(const RuntimeValue::Map& arguments) override;

    BIND_IGNORE()
    void refreshDisplayScale() override;

protected:
    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

    virtual void onTouchCaptureBegan(const sf::Vector2f& position) override;

private:
    struct PopupGeometry {
        float positionY = 0.0f;
        float height = 0.0f;
        float contentHeight = 0.0f;
        float maxScrollOffset = 0.0f;
    };

    static constexpr float RowHeight = 32.0f;
    static constexpr float ExpandedBorderHeight = 32.0f;

    static sf::Vector2f normalizedSize(const sf::Vector2f& size);
    static sf::Vector2i roundedSize(const sf::Vector2f& size);
    static std::optional<sf::Vector2f> pointerPosition(
        const RuntimeValue::Map& arguments);
    static std::optional<int> pointerButton(const RuntimeValue::Map& arguments);

    int clampedIndex(int index) const;
    float expandedHeight() const;
    PopupGeometry calculatePopupGeometry() const;
    void syncPopupGeometry(bool ensureCursor) const;
    void clampScrollOffsets() const;
    void setScrollOffset(float offset) const;
    void setScrollTargetOffset(float offset) const;
    void updateWheelScroll(float deltaTime) const;
    void ensureCursorVisible() const;
    void setExpandedState(bool expanded);
    void confirmCurrentSelection();
    bool moveCursor(int offset, bool wrap);
    bool handlePointerAction(const sf::Vector2f& screenPosition,
                             std::optional<int> button);
    std::optional<int> itemIndexAt(const sf::Vector2f& localPosition) const;
    sf::Vector2f toLocalPosition(const sf::Vector2f& screenPosition) const;
    bool hasCanvasAncestor() const;
    void restoreParentFocus();

    bool _hasOverlay() const override;
    void _drawOverlay(sf::RenderTarget& target,
                      sf::RenderStates states) const override;
    void markVisualsDirty();
    void ensureVisuals() const;
    void rebuildVisuals() const;
    void ensurePopupVisuals() const;
    void renderPopupContent() const;
    void updateSelectionVisual() const;
    void positionCollapsedText() const;
    void positionItemText(PlainText& text, int index) const;

    sf::Vector2f collapsedSize_;
    sf::Image windowSkin_;
    std::shared_ptr<PlainTextConfig> textConfig_;
    std::vector<std::string> items_;
    int selectedIndex_ = 0;
    int cursorIndex_ = 0;
    bool expanded_ = false;
    bool repeated_ = false;
    bool suppressNextClick_ = false;
    bool focusabilityOverridden_ = false;
    bool previousCanReceiveFocus_ = true;
    sf::Vector2f touchStartPosition_;
    float touchStartScrollOffset_ = 0.0f;

    std::function<void(int)> selectedIndexChangedCallback_;
    std::function<void(int)> selectionConfirmedCallback_;
    std::function<void(bool)> expandedChangedCallback_;

    std::string openSound_;
    std::string cursorSound_;
    std::string selectSound_;
    std::string cancelSound_;

    mutable bool visualsDirty_ = true;
    mutable bool popupGeometryInitialized_ = false;
    mutable PopupGeometry popupGeometry_;
    mutable float scrollOffset_ = 0.0f;
    mutable std::optional<float> scrollTargetOffset_;
    mutable sf::Vector2u popupWindowSize_;
    mutable sf::Vector2u popupContentTextureSize_;
    mutable std::unique_ptr<Rect> collapsedFrame_;
    mutable std::unique_ptr<PlainText> collapsedText_;
    mutable std::unique_ptr<Window> expandedWindow_;
    mutable std::unique_ptr<sf::RenderTexture> popupContentCanvas_;
    mutable std::unique_ptr<sf::Sprite> popupContentSprite_;
    mutable std::unique_ptr<Rect> selectionRect_;
    mutable std::vector<std::unique_ptr<PlainText>> itemTexts_;
};
