#include "UiControlAdapterSupportImpl.hpp"
#include "UiControlAdapterSupport.hpp"
#include <Runtime/AssetInputStream.hpp>
#include <UI/PlainTextConfig.hpp>
#include <UI/RichText.hpp>

#include <Text/TextConfigCodec.hpp>

#include <UI/UIState.hpp>
#include <UI/UiControlAdapterRegistry.hpp>
#include <UI/UiVector4CurveResource.hpp>

#include <Runtime/AssetStore.hpp>

#include <SFML/Graphics/Font.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>
#include <mutex>
#include <utility>

namespace ui_control_adapter_detail {

bool isNil(const UiControlPropertyValue& value) {
    return std::holds_alternative<std::monostate>(value);
}

bool requireBool(const UiControlPropertyValue& value,
                 const std::string& source) {
    return requirePropertyValue<bool>(value, source);
}

float requireFloat(const UiControlPropertyValue& value,
                   const std::string& source) {
    const std::int64_t* integer = std::get_if<std::int64_t>(&value);
    const double number = integer != nullptr
                              ? static_cast<double>(*integer)
                              : requirePropertyValue<double>(value, source);
    if (!std::isfinite(number) ||
        std::abs(number) > std::numeric_limits<float>::max()) {
        throw std::invalid_argument(source + " must be a finite float");
    }
    return static_cast<float>(number);
}

int requireInt(const UiControlPropertyValue& value, const std::string& source) {
    const std::int64_t number =
        requirePropertyValue<std::int64_t>(value, source);
    if (number < std::numeric_limits<int>::min() ||
        number > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(source + " is outside the int range");
    }
    return static_cast<int>(number);
}

const std::string& requireString(const UiControlPropertyValue& value,
                                 const std::string& source) {
    return requirePropertyValue<std::string>(value, source);
}

sf::Vector2f requireVector2f(const UiControlPropertyValue& value,
                             const std::string& source) {
    const sf::Vector2f& result =
        requirePropertyValue<sf::Vector2f>(value, source);
    if (!std::isfinite(result.x) || !std::isfinite(result.y)) {
        throw std::invalid_argument(source +
                                    " must contain finite coordinates");
    }
    return result;
}

sf::Vector2u requireVector2u(const UiControlPropertyValue& value,
                             const std::string& source) {
    return requirePropertyValue<sf::Vector2u>(value, source);
}

sf::IntRect requireIntRect(const UiControlPropertyValue& value,
                           const std::string& source) {
    return requirePropertyValue<sf::IntRect>(value, source);
}

sf::Color requireColor(const UiControlPropertyValue& value,
                       const std::string& source) {
    return requirePropertyValue<sf::Color>(value, source);
}

const UiControlPropertyValue* findValue(const UiControlProperties& properties,
                                        const std::string& name) {
    const auto found = properties.find(name);
    return found == properties.end() ? nullptr : &found->second;
}

sf::Vector2f vector2fProperty(const UiControlProperties& properties,
                              const std::string& name,
                              const sf::Vector2f& fallback) {
    const UiControlPropertyValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireVector2f(*value, name);
}

sf::Vector2u vector2uProperty(const UiControlProperties& properties,
                              const std::string& name,
                              const sf::Vector2u& fallback) {
    const UiControlPropertyValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireVector2u(*value, name);
}

std::string stringProperty(const UiControlProperties& properties,
                           const std::string& name,
                           const std::string& fallback) {
    const UiControlPropertyValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireString(*value, name);
}

std::vector<std::string> stringArrayProperty(
    const UiControlProperties& properties, const std::string& name,
    const std::vector<std::string>& fallback) {
    const UiControlPropertyValue* value = findValue(properties, name);
    if (value == nullptr) {
        return fallback;
    }
    return requirePropertyValue<std::vector<std::string>>(*value, name);
}

int intProperty(const UiControlProperties& properties, const std::string& name,
                int fallback) {
    const UiControlPropertyValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireInt(*value, name);
}

float floatProperty(const UiControlProperties& properties,
                    const std::string& name, float fallback) {
    const UiControlPropertyValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireFloat(*value, name);
}

bool boolProperty(const UiControlProperties& properties,
                  const std::string& name, bool fallback) {
    const UiControlPropertyValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireBool(*value, name);
}

sf::Color colorProperty(const UiControlProperties& properties,
                        const std::string& name, const sf::Color& fallback) {
    const UiControlPropertyValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireColor(*value, name);
}

std::optional<sf::IntRect> optionalIntRectProperty(
    const UiControlProperties& properties, const std::string& name) {
    const UiControlPropertyValue* value = findValue(properties, name);
    if (value == nullptr || isNil(*value)) {
        return std::nullopt;
    }
    return requireIntRect(*value, name);
}

PlaceholderTextureCache& placeholderTextureCache() {
    static PlaceholderTextureCache cache;
    return cache;
}

std::shared_ptr<sf::Texture> placeholderTexture() {
    PlaceholderTextureCache& cache = placeholderTextureCache();
    std::lock_guard<std::mutex> lock(cache.mutex);
    if (cache.texture == nullptr) {
        std::shared_ptr<sf::Texture> result = std::make_shared<sf::Texture>();
        const sf::Image image({1u, 1u}, sf::Color::Transparent);
        if (!result->loadFromImage(image)) {
            throw std::runtime_error(
                "Failed to create the UI placeholder texture");
        }
        cache.texture = std::move(result);
    }
    return cache.texture;
}

std::shared_ptr<sf::Texture> loadTexture(const std::string& assetKey) {
    if (assetKey.empty()) {
        return placeholderTexture();
    }
    std::unique_ptr<ludork::runtime::AssetInputStream> stream =
        ludork::runtime::assetStore().open(assetKey);
    std::shared_ptr<sf::Texture> texture = std::make_shared<sf::Texture>();
    if (!texture->loadFromStream(*stream)) {
        throw std::runtime_error("Failed to load UI texture: " + assetKey);
    }
    return texture;
}

std::shared_ptr<sf::Texture> loadOptionalTexture(const std::string& assetKey) {
    return assetKey.empty() ? nullptr : loadTexture(assetKey);
}

sf::Image loadWindowSkin(const std::string& requestedKey) {
    std::string assetKey = requestedKey;
    if (assetKey.empty() && defaultWindowskinName.has_value()) {
        assetKey = *defaultWindowskinName;
    }
    if (assetKey.empty()) {
        return sf::Image({192u, 128u}, sf::Color::Transparent);
    }
    std::unique_ptr<ludork::runtime::AssetInputStream> stream =
        ludork::runtime::assetStore().open(assetKey);
    sf::Image image;
    if (!image.loadFromStream(*stream)) {
        throw std::runtime_error("Failed to load UI window skin: " + assetKey);
    }
    return image;
}

std::shared_ptr<sf::Font> loadFont(const std::string& fontKey,
                                   const std::string& source) {
    return ludork::engine::text_config::loadFont(fontKey, source);
}

sf::Text::LineAlignment lineAlignment(const std::string& value,
                                      const std::string& source) {
    return ludork::engine::text_config::parseLineAlignment(value, source);
}

std::shared_ptr<PlainTextConfig> plainTextConfig(
    const std::string& textConfigKey) {
    return ludork::engine::text_config::loadPlain(textConfigKey);
}

std::shared_ptr<PlainTextConfig> plainTextControlConfig(
    const UiControlProperties& properties) {
    const std::string textConfigKey = stringProperty(properties, "textConfig");
    if (!textConfigKey.empty()) {
        return plainTextConfig(textConfigKey);
    }
    std::shared_ptr<PlainTextConfig> result =
        std::make_shared<PlainTextConfig>();
    result->name = "Inline UI Text";
    result->font = loadFont(stringProperty(properties, "font"), result->name);
    const int characterSize = intProperty(properties, "characterSize", 22);
    if (characterSize < 1 || characterSize > 512) {
        throw std::invalid_argument("characterSize must be between 1 and 512");
    }
    result->characterSize = static_cast<unsigned int>(characterSize);
    result->style = sf::Text::Regular;
    if (boolProperty(properties, "bold", false)) {
        result->style |= sf::Text::Bold;
    }
    if (boolProperty(properties, "italic", false)) {
        result->style |= sf::Text::Italic;
    }
    if (boolProperty(properties, "underlined", false)) {
        result->style |= sf::Text::Underlined;
    }
    if (boolProperty(properties, "strikeThrough", false)) {
        result->style |= sf::Text::StrikeThrough;
    }
    result->slantAngle = floatProperty(properties, "slantAngle", 0.0f);
    if (result->slantAngle < -45.0f || result->slantAngle > 45.0f) {
        throw std::invalid_argument("slantAngle must be between -45 and 45");
    }
    result->fillColor =
        colorProperty(properties, "fillColor", sf::Color::White);
    result->letterSpacing = floatProperty(properties, "letterSpacing", 1.0f);
    result->lineSpacing = floatProperty(properties, "lineSpacing", 1.0f);
    if (result->letterSpacing < 0.1f || result->letterSpacing > 10.0f ||
        result->lineSpacing < 0.1f || result->lineSpacing > 10.0f) {
        throw std::invalid_argument(
            "letterSpacing and lineSpacing must be between 0.1 and 10");
    }
    result->lineAlignment =
        lineAlignment(stringProperty(properties, "lineAlignment", "default"),
                      "lineAlignment");
    result->outline.color =
        colorProperty(properties, "outlineColor", sf::Color::Black);
    result->outline.thickness =
        floatProperty(properties, "outlineThickness", 0.0f);
    if (result->outline.thickness < 0.0f || result->outline.thickness > 32.0f) {
        throw std::invalid_argument(
            "outlineThickness must be between 0 and 32");
    }
    result->glow.enabled = boolProperty(properties, "glowEnabled", false);
    result->glow.color =
        colorProperty(properties, "glowColor", sf::Color::Transparent);
    result->glow.radius = floatProperty(properties, "glowRadius", 0.0f);
    result->glow.intensity = floatProperty(properties, "glowIntensity", 0.0f);
    if (result->glow.radius < 0.0f || result->glow.radius > 64.0f ||
        result->glow.intensity < 0.0f || result->glow.intensity > 1.0f) {
        throw std::invalid_argument(
            "glowRadius must be between 0 and 64 and glowIntensity between 0 "
            "and 1");
    }
    result->gradient.enabled =
        boolProperty(properties, "gradientEnabled", false);
    result->gradient.direction =
        stringProperty(properties, "gradientDirection", "vertical");
    if (result->gradient.direction != "vertical" &&
        result->gradient.direction != "horizontal") {
        throw std::invalid_argument(
            "gradientDirection must be vertical or horizontal");
    }
    if (result->gradient.enabled) {
        const std::string curveKey =
            stringProperty(properties, "gradientCurve");
        if (curveKey.empty()) {
            throw std::invalid_argument(
                "gradientCurve is required when gradientEnabled is true");
        }
        result->gradient.curve = loadUiVector4CurveResource(curveKey);
    }
    return result;
}

std::shared_ptr<RichText::RichTextConfig> richTextConfig(
    const std::string& textConfigKey) {
    if (textConfigKey.empty()) {
        std::shared_ptr<RichText::RichTextConfig> result =
            std::make_shared<RichText::RichTextConfig>();
        result->name = "UI Asset Default";
        result->font = loadFont({}, result->name);
        return result;
    }
    return ludork::engine::text_config::loadRich(textConfigKey);
}

void arrangeByScale(ControlBase& control, const sf::Vector2f& size,
                    const sf::Vector2f& renderScale) {
    const sf::Vector2f desired = control.getSize();
    sf::Vector2f scale = renderScale;
    if (desired.x > 0.0f) {
        scale.x *= std::max(0.0f, size.x) / desired.x;
    }
    if (desired.y > 0.0f) {
        scale.y *= std::max(0.0f, size.y) / desired.y;
    }
    control.setScale(scale);
}

void arrangeAtIntrinsicSize(ControlBase& control,
                            const sf::Vector2f& renderScale) {
    control.setScale(renderScale);
}

}  // namespace ui_control_adapter_detail

void clearUiControlAdapterResourceCache() noexcept {
    ui_control_adapter_detail::PlaceholderTextureCache& cache =
        ui_control_adapter_detail::placeholderTextureCache();
    std::lock_guard<std::mutex> lock(cache.mutex);
    cache.texture.reset();
}
