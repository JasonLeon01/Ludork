#pragma once

#include <UI/TextGlowConfig.hpp>
#include <UI/TextGradientConfig.hpp>

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <UI/ControlBase.hpp>
#include <UI/TextStyle.hpp>

BIND_CLASS(callbacks = true)
class RichText : public ControlBase {
public:
    LUDORK_CAST_DERIVED(RichText, ControlBase)

    BIND_CLASS(copyable = true, table_init = true)
    struct LUDORK_ENGINE_API RichTextConfig : public RuntimeObject {
        LUDORK_CAST_DERIVED(RichTextConfig, RuntimeObject)

        BIND_PROPERTY()
        std::string type = "richTextConfig";

        BIND_PROPERTY()
        std::string name;

        BIND_PROPERTY()
        std::shared_ptr<sf::Font> font;

        BIND_PROPERTY()
        sf::Text::LineAlignment lineAlignment =
            sf::Text::LineAlignment::Default;

        BIND_PROPERTY()
        std::shared_ptr<TextStyle> defaultStyle;

        BIND_PROPERTY()
        std::vector<std::string> styleOrder;

        BIND_PROPERTY()
        std::unordered_map<std::string, std::shared_ptr<TextStyle>> styles;

        BIND_PROPERTY()
        TextGlowConfig glow;

        BIND_PROPERTY()
        TextGradientConfig gradient;
    };

    BIND_INIT()
    RichText(std::shared_ptr<RichText::RichTextConfig> config,
             const std::string& text);
    virtual ~RichText();

    RichText(const RichText&) = delete;
    RichText& operator=(const RichText&) = delete;
    RichText(RichText&&) = delete;
    RichText& operator=(RichText&&) = delete;

    BIND_METHOD(Pure = true)
    std::shared_ptr<RichText::RichTextConfig> getConfig() const;

    BIND_METHOD()
    void setString(const std::string& text);

    BIND_METHOD(Pure = true)
    const std::string& getString() const;

    BIND_METHOD()
    void setColour(const sf::Color& colour);

    BIND_METHOD(Pure = true)
    sf::Color getColour() const;

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

    void refreshDisplayScale() override;

protected:
    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

private:
    struct Segment {
        std::unique_ptr<sf::Text> text;
        TextStyle style;
    };

    struct EffectCache;

    static const RichText::RichTextConfig& configReference(
        const std::shared_ptr<RichText::RichTextConfig>& config);
    static std::shared_ptr<RichText::RichTextConfig> snapshotConfig(
        const std::shared_ptr<RichText::RichTextConfig>& config);
    static std::shared_ptr<RichText::RichTextConfig> snapshotConfig(
        const RichText::RichTextConfig& config);
    static std::optional<sf::Color> parseMarkerColour(
        const std::string& marker);
    static sf::Color modulateColour(const sf::Color& baseColour,
                                    const sf::Color& factorColour);
    sf::Color presentedColour() const;

    void renderText(const std::string& text);
    TextStyle createDefaultStyle() const;
    std::optional<TextStyle> resolveStyleMarker(
        const std::string& marker) const;
    std::unique_ptr<sf::Text> buildText(const std::string& text,
                                        const TextStyle& style) const;
    static float getBaseline(const sf::Text& text);
    static float measureAdvance(const sf::Text& text);
    float getLineAdvance(const TextStyle& style) const;
    void refreshSegmentColours();
    void _refreshPresentationColour() override;
    void applySegmentColour(sf::Text& text, const TextStyle& style) const;
    void invalidateEffects();
    void syncDisplayScale() const;
    void ensureEffects() const;
    sf::FloatRect getPixelBounds() const;

    std::shared_ptr<const RichText::RichTextConfig> config_;
    sf::Color colour_ = sf::Color::White;
    std::string string_;
    std::vector<Segment> segments_;
    sf::FloatRect localBounds_;
    mutable std::unique_ptr<EffectCache> effects_;
    float displayScale_ = 1.0f;
};
