#pragma once

#include <BindAnnotations.hpp>
#include <Curve.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <UI/ControlBase.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Text.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

BIND_CLASS(copyable = true, table_init = true)
struct TextOutlineConfig {
    BIND_PROPERTY()
    sf::Color color = sf::Color::Black;

    BIND_PROPERTY()
    float thickness = 0.0f;
};

BIND_CLASS(copyable = true, table_init = true)
struct TextGlowConfig {
    BIND_PROPERTY()
    bool enabled = false;

    BIND_PROPERTY()
    sf::Color color = sf::Color::Transparent;

    BIND_PROPERTY()
    float radius = 0.0f;

    BIND_PROPERTY()
    float intensity = 0.0f;
};

BIND_CLASS(copyable = true, table_init = true)
struct TextGradientConfig {
    BIND_PROPERTY()
    bool enabled = false;

    BIND_PROPERTY()
    std::string direction = "vertical";

    BIND_PROPERTY()
    std::shared_ptr<Vector4Curve> curve;
};

BIND_CLASS(copyable = true, table_init = true)
struct TextStyle {
    BIND_PROPERTY()
    std::optional<unsigned int> characterSize;

    BIND_PROPERTY()
    std::optional<bool> bold;

    BIND_PROPERTY()
    std::optional<bool> italic;

    BIND_PROPERTY()
    std::optional<bool> underlined;

    BIND_PROPERTY()
    std::optional<bool> strikeThrough;

    BIND_PROPERTY()
    std::optional<sf::Color> fillColor;

    BIND_PROPERTY()
    std::optional<float> letterSpacing;

    BIND_PROPERTY()
    std::optional<float> lineSpacing;

    BIND_PROPERTY()
    std::optional<sf::Color> outlineColor;

    BIND_PROPERTY()
    std::optional<float> outlineThickness;

    BIND_METHOD()
    void enableStyle(sf::Text& text) const;

    BIND_METHOD()
    void adaptStyle(const TextStyle& inStyle);

    BIND_METHOD(name = "_copy", metadata = false)
    TextStyle copy() const;
};

BIND_CLASS(copyable = true, table_init = true)
struct LUDORK_ENGINE_API PlainTextConfig : public RuntimeObject {
    BIND_PROPERTY()
    std::string type = "plainTextConfig";

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    std::shared_ptr<sf::Font> font;

    BIND_PROPERTY()
    unsigned int characterSize = 30;

    BIND_PROPERTY()
    std::uint32_t style = sf::Text::Regular;

    BIND_PROPERTY()
    float slantAngle = 0.0f;

    BIND_PROPERTY()
    sf::Color fillColor = sf::Color::White;

    BIND_PROPERTY()
    float letterSpacing = 1.0f;

    BIND_PROPERTY()
    float lineSpacing = 1.0f;

    BIND_PROPERTY()
    sf::Text::LineAlignment lineAlignment = sf::Text::LineAlignment::Default;

    BIND_PROPERTY()
    TextOutlineConfig outline;

    BIND_PROPERTY()
    TextGlowConfig glow;

    BIND_PROPERTY()
    TextGradientConfig gradient;
};

BIND_CLASS(copyable = true, table_init = true)
struct LUDORK_ENGINE_API RichTextConfig : public RuntimeObject {
    BIND_PROPERTY()
    std::string type = "richTextConfig";

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    std::shared_ptr<sf::Font> font;

    BIND_PROPERTY()
    sf::Text::LineAlignment lineAlignment = sf::Text::LineAlignment::Default;

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

BIND_CLASS(
    callbacks =
        "getSize,getLocalBounds,getGlobalBounds,getOrigin,setOrigin,draw")
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

    BIND_IGNORE()
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
    void refreshDirectColours();
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

BIND_CLASS(
    callbacks =
        "getSize,getLocalBounds,getGlobalBounds,getOrigin,setOrigin,draw")
class RichText : public ControlBase {
public:
    BIND_INIT()
    RichText(std::shared_ptr<RichTextConfig> config, const std::string& text);
    virtual ~RichText();

    RichText(const RichText&) = delete;
    RichText& operator=(const RichText&) = delete;
    RichText(RichText&&) = delete;
    RichText& operator=(RichText&&) = delete;

    BIND_METHOD(Pure = true)
    std::shared_ptr<RichTextConfig> getConfig() const;

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

    BIND_IGNORE()
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

    static const RichTextConfig& configReference(
        const std::shared_ptr<RichTextConfig>& config);
    static std::shared_ptr<RichTextConfig> snapshotConfig(
        const std::shared_ptr<RichTextConfig>& config);
    static std::shared_ptr<RichTextConfig> snapshotConfig(
        const RichTextConfig& config);
    static std::optional<sf::Color> parseMarkerColour(
        const std::string& marker);
    static sf::Color modulateColour(const sf::Color& baseColour,
                                    const sf::Color& factorColour);

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
    void applySegmentColour(sf::Text& text, const TextStyle& style) const;
    void invalidateEffects();
    void syncDisplayScale() const;
    void ensureEffects() const;
    sf::FloatRect getPixelBounds() const;

    std::shared_ptr<const RichTextConfig> config_;
    sf::Color colour_ = sf::Color::White;
    std::string string_;
    std::vector<Segment> segments_;
    sf::FloatRect localBounds_;
    mutable std::unique_ptr<EffectCache> effects_;
    float displayScale_ = 1.0f;
};
