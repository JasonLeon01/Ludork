#pragma once

#include <UI/ControlBase.hpp>
#include <UI/FunctionalBase.hpp>

struct PlainTextConfig;

/// A single-line UTF-8 field. Keyboard focus alone does not start editing.
BIND_CLASS()
class LUDORK_ENGINE_API TextBox : public ControlBase, public FunctionalBase {
public:
    LUDORK_CAST_DERIVED(TextBox, ControlBase, FunctionalBase)

    BIND_INIT()
    TextBox(const sf::Vector2f& size, const sf::Image& windowSkin,
            std::shared_ptr<PlainTextConfig> textConfig,
            const std::string& text = "");
    ~TextBox() override;

    BIND_METHOD(Pure = true)
    sf::Vector2f getSize() const override;
    BIND_METHOD(Pure = true)
    sf::FloatRect getLocalBounds() const override;
    BIND_METHOD()
    void resize(const sf::Vector2f& size);
    BIND_METHOD()
    void setWindowSkin(const sf::Image& windowSkin);
    BIND_METHOD()
    void setTextConfig(std::shared_ptr<PlainTextConfig> textConfig);
    BIND_METHOD(Pure = true)
    std::string getString() const;
    BIND_METHOD()
    void setString(const std::string& text);
    BIND_METHOD(Pure = true)
    bool isEditing() const;
    BIND_METHOD()
    bool beginEdit();
    BIND_METHOD()
    void finishEdit();
    BIND_METHOD()
    void cancelEdit();
    BIND_METHOD()
    void setInputDialogLabels(const std::string& title, const std::string& done,
                              const std::string& cancel);
    /// Fires once per changed committed value, including setString and
    /// cancellation. Marked IME text is provisional and does not change
    /// getString(). Pass nil to clear.
    BIND_METHOD()
    void setOnTextChanged(
        std::optional<std::function<void(const std::string&)>> callback);
    /// Fires when an edit session starts or ends. Pass nil to clear.
    BIND_METHOD()
    void setOnEditingChanged(std::optional<std::function<void(bool)>> callback);
    BIND_METHOD()
    void update(float deltaTime) override;
    BIND_METHOD()
    void onConfirm(const UiInputEventArguments& arguments) override;
    BIND_METHOD()
    void onCancel(const UiInputEventArguments& arguments) override;
    BIND_METHOD()
    void onClick(const UiInputEventArguments& arguments) override;
    BIND_METHOD()
    bool onMouseButtonDown(const UiInputEventArguments& arguments) override;
    BIND_METHOD()
    void onMouseMoved(const UiInputEventArguments& arguments) override;
    BIND_METHOD()
    void onKeyDown(const UiInputEventArguments& arguments) override;
    void refreshDisplayScale() override;
    void releaseRuntimeCallbacks() noexcept override;

protected:
    void onFocusLost() override;
    void onInteractionInvalidated() override;
    void _refreshPresentationColour() override;
    BIND_METHOD()
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void updateLayout();
    void placeCaret(const sf::Vector2f& position, bool extend);
    void updateCaretRect();
};
