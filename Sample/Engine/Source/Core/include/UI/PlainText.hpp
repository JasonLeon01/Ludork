#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <UI/ControlBase.hpp>

struct PlainTextConfig;

BIND_CLASS(callbacks = true)
class PlainText : public ControlBase {
public:
    BIND_INIT()
    PlainText(std::shared_ptr<PlainTextConfig> config, const std::string& text);
    virtual ~PlainText();

    PlainText(const PlainText&) = delete;
    PlainText& operator=(const PlainText&) = delete;
    PlainText(PlainText&&) = delete;
    PlainText& operator=(PlainText&&) = delete;

    BIND_METHOD(Pure = true)
    std::shared_ptr<PlainTextConfig> getConfig() const;

    BIND_METHOD(Pure = true)
    unsigned int getCharacterSize() const;

    BIND_METHOD()
    void setString(const std::string& text);

    BIND_METHOD(Pure = true)
    std::string getString() const;

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getLocalBounds() const override;

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getGlobalBounds() const;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getOrigin() const override;

    BIND_METHOD()
    virtual void setOrigin(const sf::Vector2f& origin) override;

    BIND_METHOD(Pure = true)
    sf::Color getColour() const;

    BIND_METHOD()
    void setColour(const sf::Color& colour);

    void refreshDisplayScale() override;

protected:
    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

private:
    struct EffectCache;

    static const PlainTextConfig& configReference(
        const std::shared_ptr<PlainTextConfig>& config);
    static std::shared_ptr<PlainTextConfig> snapshotConfig(
        const std::shared_ptr<PlainTextConfig>& config);
    static std::shared_ptr<PlainTextConfig> snapshotConfig(
        const PlainTextConfig& config);
    void applyConfig();
    sf::Color presentedColour() const;
    void refreshDirectColours();
    void _refreshPresentationColour() override;
    void invalidateEffects();
    void syncDisplayScale() const;
    sf::FloatRect getPixelBounds() const;
    void ensureEffects() const;

    std::shared_ptr<const PlainTextConfig> config_;
    sf::Color colour_ = sf::Color::White;
    sf::Text text_;
    mutable std::unique_ptr<EffectCache> effects_;
    float displayScale_ = 1.0f;
};
