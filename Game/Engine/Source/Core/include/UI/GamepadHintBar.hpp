#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

#include <Input/InputNamedValue.hpp>
#include <UI/ControlBase.hpp>
#include <UI/FunctionalBase.hpp>
#include <UI/PlainText.hpp>
#include <UI/PlainTextConfig.hpp>

namespace ludork::engine::ui_interaction {
class GamepadKeyHintImpl;
}

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API GamepadHintBar : public ControlBase,
                                         public FunctionalBase {
public:
    LUDORK_CAST_DERIVED(GamepadHintBar, ControlBase, FunctionalBase)

    BIND_CLASS(copyable = true, table_init = true, strict_fields = true)
    struct GamepadHint {
        BIND_PROPERTY()
        InputNamedValue Button;

        BIND_PROPERTY()
        bool LongPress = false;

        BIND_PROPERTY()
        std::string Text;
    };

    BIND_INIT()
    GamepadHintBar(const sf::Vector2f& size,
                   std::shared_ptr<PlainTextConfig> textConfig);
    virtual ~GamepadHintBar();

    GamepadHintBar(const GamepadHintBar&) = delete;
    GamepadHintBar& operator=(const GamepadHintBar&) = delete;
    GamepadHintBar(GamepadHintBar&&) = delete;
    GamepadHintBar& operator=(GamepadHintBar&&) = delete;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD()
    void resize(const sf::Vector2f& size);

    BIND_METHOD()
    void setTextConfig(std::shared_ptr<PlainTextConfig> textConfig);

    BIND_METHOD()
    void setHints(const std::vector<GamepadHint>& hints);

    BIND_METHOD(Pure = true)
    int getHintCount() const;

    BIND_METHOD()
    void setHintEnabled(int index, bool enabled);

    BIND_METHOD(Pure = true)
    bool isHintEnabled(int index) const;

    BIND_METHOD(Pure = true)
    bool isGamepadConnected() const;

    BIND_METHOD()
    void setOnHintTriggered(std::optional<std::function<void(int)>> callback);

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getLocalBounds() const override;

    BIND_METHOD()
    virtual void update(float deltaTime) override;

    void refreshDisplayScale() override;

    void releaseRuntimeCallbacks() noexcept override;

protected:
    void _refreshPresentationColour() override;

    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

private:
    struct Entry {
        InputNamedValue button;
        bool longPress = false;
        bool enabled = true;
        std::unique_ptr<ludork::engine::ui_interaction::GamepadKeyHintImpl>
            keyHint;
        std::unique_ptr<PlainText> label;
    };

    static sf::Vector2f normalizedSize(const sf::Vector2f& size);

    Entry& requireEntry(int index);
    const Entry& requireEntry(int index) const;
    void rebuildEntries(const std::vector<GamepadHint>& hints);
    void layoutEntries();
    void applyEntryColours(Entry& entry);
    void applyPresentationColour();
    void drawEntry(const Entry& entry, sf::RenderTarget& target,
                   const sf::RenderStates& states) const;

    sf::Vector2f size_;
    std::shared_ptr<PlainTextConfig> textConfig_;
    std::vector<Entry> entries_;
    std::function<void(int)> hintTriggeredCallback_;
};
