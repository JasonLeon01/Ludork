#include "UiControlAdapterSupport.hpp"

#include <Curve.hpp>
#include <UI/UIState.hpp>
#include <UI/UiControlAdapterRegistry.hpp>
#include <UI/UiVector4CurveResource.hpp>
#include <Utils/File.hpp>

#include <Utf8Path.hpp>

#include <SFML/Graphics/Font.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <mutex>
#include <utility>

namespace ui_control_adapter_detail {

const RuntimeValue* findValue(const RuntimeValue::Map& values,
                              const std::string& name) {
    const auto iterator = values.find(name);
    return iterator == values.end() ? nullptr : &iterator->second;
}

const RuntimeValue::Map& requireMap(const RuntimeValue& value,
                                    const std::string& source) {
    const RuntimeValue::Map* map = value.getIf<RuntimeValue::Map>();
    if (map == nullptr) {
        throw std::invalid_argument(source + " must be an object");
    }
    return *map;
}

const RuntimeValue::Array& requireArray(const RuntimeValue& value,
                                        const std::string& source) {
    const RuntimeValue::Array* array = value.getIf<RuntimeValue::Array>();
    if (array == nullptr) {
        throw std::invalid_argument(source + " must be an array");
    }
    return *array;
}

double requireNumber(const RuntimeValue& value, const std::string& source) {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return static_cast<double>(*integer);
    }
    if (const double* number = value.getIf<double>()) {
        if (std::isfinite(*number)) {
            return *number;
        }
    }
    throw std::invalid_argument(source + " must be a finite number");
}

float requireFloat(const RuntimeValue& value, const std::string& source) {
    const double number = requireNumber(value, source);
    if (number < -static_cast<double>(std::numeric_limits<float>::max()) ||
        number > static_cast<double>(std::numeric_limits<float>::max())) {
        throw std::invalid_argument(source + " is outside the float range");
    }
    return static_cast<float>(number);
}

std::int64_t requireInteger(const RuntimeValue& value,
                            const std::string& source) {
    const std::int64_t* integer = value.getIf<std::int64_t>();
    if (integer == nullptr) {
        throw std::invalid_argument(source + " must be an integer");
    }
    return *integer;
}

int requireInt(const RuntimeValue& value, const std::string& source) {
    const std::int64_t integer = requireInteger(value, source);
    if (integer < std::numeric_limits<int>::min() ||
        integer > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(source + " is outside the int range");
    }
    return static_cast<int>(integer);
}

unsigned int requireUnsigned(const RuntimeValue& value,
                             const std::string& source) {
    const std::int64_t integer = requireInteger(value, source);
    if (integer < 0 || static_cast<std::uint64_t>(integer) >
                           std::numeric_limits<unsigned int>::max()) {
        throw std::invalid_argument(source + " must be an unsigned integer");
    }
    return static_cast<unsigned int>(integer);
}

bool requireBool(const RuntimeValue& value, const std::string& source) {
    const bool* boolean = value.getIf<bool>();
    if (boolean == nullptr) {
        throw std::invalid_argument(source + " must be a boolean");
    }
    return *boolean;
}

const std::string& requireString(const RuntimeValue& value,
                                 const std::string& source) {
    const std::string* text = value.getIf<std::string>();
    if (text == nullptr) {
        throw std::invalid_argument(source + " must be a string");
    }
    return *text;
}

sf::Vector2f requireVector2f(const RuntimeValue& value,
                             const std::string& source) {
    const RuntimeValue::Array& array = requireArray(value, source);
    if (array.size() != 2) {
        throw std::invalid_argument(source + " must contain two numbers");
    }
    return {requireFloat(array[0], source + "[0]"),
            requireFloat(array[1], source + "[1]")};
}

sf::Vector2u requireVector2u(const RuntimeValue& value,
                             const std::string& source) {
    const RuntimeValue::Array& array = requireArray(value, source);
    if (array.size() != 2) {
        throw std::invalid_argument(source + " must contain two integers");
    }
    return {requireUnsigned(array[0], source + "[0]"),
            requireUnsigned(array[1], source + "[1]")};
}

sf::IntRect requireIntRect(const RuntimeValue& value,
                           const std::string& source) {
    const RuntimeValue::Array& array = requireArray(value, source);
    if (array.size() != 4) {
        throw std::invalid_argument(source + " must contain four integers");
    }
    return {{requireInt(array[0], source + "[0]"),
             requireInt(array[1], source + "[1]")},
            {requireInt(array[2], source + "[2]"),
             requireInt(array[3], source + "[3]")}};
}

sf::Color requireColor(const RuntimeValue& value, const std::string& source) {
    const RuntimeValue::Array& array = requireArray(value, source);
    if (array.size() != 3 && array.size() != 4) {
        throw std::invalid_argument(
            source + " must contain three or four integer channels");
    }
    auto channel = [&](std::size_t index) {
        const std::int64_t number = requireInteger(
            array[index], source + "[" + std::to_string(index) + "]");
        if (number < 0 || number > 255) {
            throw std::invalid_argument(source +
                                        " channels must be between 0 and 255");
        }
        return static_cast<std::uint8_t>(number);
    };
    return {channel(0), channel(1), channel(2),
            array.size() == 4 ? channel(3) : std::uint8_t{255}};
}

sf::Vector2f vector2fProperty(const RuntimeValue::Map& properties,
                              const std::string& name,
                              const sf::Vector2f& fallback) {
    const RuntimeValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireVector2f(*value, name);
}

sf::Vector2u vector2uProperty(const RuntimeValue::Map& properties,
                              const std::string& name,
                              const sf::Vector2u& fallback) {
    const RuntimeValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireVector2u(*value, name);
}

std::string stringProperty(const RuntimeValue::Map& properties,
                           const std::string& name,
                           const std::string& fallback) {
    const RuntimeValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireString(*value, name);
}

int intProperty(const RuntimeValue::Map& properties, const std::string& name,
                int fallback) {
    const RuntimeValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireInt(*value, name);
}

float floatProperty(const RuntimeValue::Map& properties,
                    const std::string& name, float fallback) {
    const RuntimeValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireFloat(*value, name);
}

bool boolProperty(const RuntimeValue::Map& properties, const std::string& name,
                  bool fallback) {
    const RuntimeValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireBool(*value, name);
}

sf::Color colorProperty(const RuntimeValue::Map& properties,
                        const std::string& name, const sf::Color& fallback) {
    const RuntimeValue* value = findValue(properties, name);
    return value == nullptr ? fallback : requireColor(*value, name);
}

std::optional<sf::IntRect> optionalIntRectProperty(
    const RuntimeValue::Map& properties, const std::string& name) {
    const RuntimeValue* value = findValue(properties, name);
    if (value == nullptr || value->isNil()) {
        return std::nullopt;
    }
    return requireIntRect(*value, name);
}

std::filesystem::path safeAssetPath(const std::string& value,
                                    const std::string& defaultFolder) {
    std::filesystem::path relative =
        ludork::standard::pathFromUtf8(value).lexically_normal();
    if (relative.is_absolute()) {
        throw std::invalid_argument("UI resource path must be relative: " +
                                    value);
    }
    for (const std::filesystem::path& part : relative) {
        if (part == "..") {
            throw std::invalid_argument(
                "UI resource path cannot traverse parent directories: " +
                value);
        }
    }
    const std::filesystem::path assetsRoot = "Assets";
    if (!relative.empty() && *relative.begin() == assetsRoot) {
        return std::filesystem::path(".") / relative;
    }
    if (relative.parent_path().empty() && !defaultFolder.empty()) {
        relative = ludork::standard::pathFromUtf8(defaultFolder) / relative;
    }
    return std::filesystem::path(".") / assetsRoot / relative;
}

struct PlaceholderTextureCache {
    std::mutex mutex;
    std::shared_ptr<sf::Texture> texture;
};

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
    const std::filesystem::path path = safeAssetPath(assetKey, {});
    std::shared_ptr<sf::Texture> texture = std::make_shared<sf::Texture>();
    if (!texture->loadFromFile(path)) {
        throw std::runtime_error("Failed to load UI texture: " +
                                 ludork::standard::pathToUtf8(path));
    }
    return texture;
}

sf::Image loadWindowSkin(const std::string& requestedKey) {
    std::string assetKey = requestedKey;
    if (assetKey.empty() && defaultWindowskinName.has_value()) {
        assetKey = *defaultWindowskinName;
    }
    if (assetKey.empty()) {
        return sf::Image({192u, 128u}, sf::Color::Transparent);
    }
    const std::filesystem::path path = safeAssetPath(assetKey, "System");
    sf::Image image;
    if (!image.loadFromFile(path)) {
        throw std::runtime_error("Failed to load UI window skin: " +
                                 ludork::standard::pathToUtf8(path));
    }
    return image;
}

std::shared_ptr<sf::Font> loadFont(const std::string& fontKey,
                                   const std::string& source) {
    if (fontKey.empty()) {
        if (defaultFont == nullptr) {
            throw std::runtime_error(
                "Default font is unavailable for UI text config " + source);
        }
        return defaultFont;
    }
    const std::filesystem::path path = safeAssetPath(fontKey, "Fonts");
    std::shared_ptr<sf::Font> font = std::make_shared<sf::Font>();
    if (!font->openFromFile(path)) {
        throw std::runtime_error("Failed to load UI font: " +
                                 ludork::standard::pathToUtf8(path));
    }
    return font;
}

sf::Text::LineAlignment lineAlignment(const std::string& value,
                                      const std::string& source) {
    if (value == "default") {
        return sf::Text::LineAlignment::Default;
    }
    if (value == "left") {
        return sf::Text::LineAlignment::Left;
    }
    if (value == "center") {
        return sf::Text::LineAlignment::Center;
    }
    if (value == "right") {
        return sf::Text::LineAlignment::Right;
    }
    throw std::invalid_argument(source + " has invalid alignment " + value);
}

std::uint32_t textStyle(const RuntimeValue& value, const std::string& source) {
    const RuntimeValue::Map& map = requireMap(value, source);
    std::uint32_t style = sf::Text::Regular;
    auto enable = [&](const std::string& name, std::uint32_t flag) {
        const RuntimeValue* setting = findValue(map, name);
        if (setting != nullptr && requireBool(*setting, source + "." + name)) {
            style |= flag;
        }
    };
    enable("bold", sf::Text::Bold);
    enable("italic", sf::Text::Italic);
    enable("underlined", sf::Text::Underlined);
    enable("strikeThrough", sf::Text::StrikeThrough);
    return style;
}

void applyGlow(TextGlowConfig& target, const RuntimeValue* value,
               const std::string& source) {
    if (value == nullptr) {
        return;
    }
    const RuntimeValue::Map& map = requireMap(*value, source);
    if (const RuntimeValue* enabled = findValue(map, "enabled")) {
        target.enabled = requireBool(*enabled, source + ".enabled");
    }
    if (const RuntimeValue* color = findValue(map, "color")) {
        target.color = requireColor(*color, source + ".color");
    }
    if (const RuntimeValue* radius = findValue(map, "radius")) {
        target.radius = requireFloat(*radius, source + ".radius");
    }
    if (const RuntimeValue* intensity = findValue(map, "intensity")) {
        target.intensity = requireFloat(*intensity, source + ".intensity");
    }
}

void applyGradient(TextGradientConfig& target, const RuntimeValue* value,
                   const std::string& source) {
    if (value == nullptr) {
        return;
    }
    const RuntimeValue::Map& map = requireMap(*value, source);
    if (const RuntimeValue* enabled = findValue(map, "enabled")) {
        target.enabled = requireBool(*enabled, source + ".enabled");
    }
    if (const RuntimeValue* direction = findValue(map, "direction")) {
        target.direction = requireString(*direction, source + ".direction");
    }
    const RuntimeValue* curve = findValue(map, "curve");
    if (target.enabled) {
        const std::string curveKey =
            curve == nullptr ? std::string()
                             : requireString(*curve, source + ".curve");
        if (curveKey.empty()) {
            throw std::invalid_argument(source +
                                        " requires a curve when enabled");
        }
        target.curve = loadUiVector4CurveResource(curveKey);
    }
}

RuntimeValue::Map loadTextConfigData(const std::string& textConfigKey) {
    std::filesystem::path relative =
        ludork::standard::pathFromUtf8(textConfigKey);
    if (relative.is_absolute()) {
        throw std::invalid_argument("UI text config key must be relative: " +
                                    textConfigKey);
    }
    for (const std::filesystem::path& part : relative) {
        if (part == "..") {
            throw std::invalid_argument(
                "UI text config key cannot traverse parent directories: " +
                textConfigKey);
        }
    }
    if (relative.extension().empty()) {
        relative += ".json";
    }
    const std::filesystem::path path =
        std::filesystem::path(".") / "Data" / "TextConfigs" / relative;
    return requireMap(getJSONData(path), ludork::standard::pathToUtf8(path));
}

std::shared_ptr<PlainTextConfig> plainTextConfig(
    const std::string& textConfigKey) {
    std::shared_ptr<PlainTextConfig> result =
        std::make_shared<PlainTextConfig>();
    if (textConfigKey.empty()) {
        result->name = "UI Asset Default";
        result->font = loadFont({}, result->name);
        result->characterSize =
            static_cast<unsigned int>(std::max(1, defaultFontSize));
        return result;
    }

    const RuntimeValue::Map data = loadTextConfigData(textConfigKey);
    const RuntimeValue* type = findValue(data, "type");
    if (type == nullptr ||
        requireString(*type, textConfigKey + ".type") != "plainTextConfig") {
        throw std::invalid_argument("UI text config is not plain text: " +
                                    textConfigKey);
    }
    result->name = stringProperty(data, "name", textConfigKey);
    result->font = loadFont(stringProperty(data, "font"), textConfigKey);
    if (const RuntimeValue* characterSize = findValue(data, "characterSize")) {
        result->characterSize =
            requireUnsigned(*characterSize, textConfigKey + ".characterSize");
    }
    if (const RuntimeValue* style = findValue(data, "style")) {
        result->style = textStyle(*style, textConfigKey + ".style");
    }
    if (const RuntimeValue* slantAngle = findValue(data, "slantAngle")) {
        result->slantAngle =
            requireFloat(*slantAngle, textConfigKey + ".slantAngle");
    }
    if (const RuntimeValue* fillColor = findValue(data, "fillColor")) {
        result->fillColor =
            requireColor(*fillColor, textConfigKey + ".fillColor");
    }
    if (const RuntimeValue* letterSpacing = findValue(data, "letterSpacing")) {
        result->letterSpacing =
            requireFloat(*letterSpacing, textConfigKey + ".letterSpacing");
    }
    if (const RuntimeValue* lineSpacing = findValue(data, "lineSpacing")) {
        result->lineSpacing =
            requireFloat(*lineSpacing, textConfigKey + ".lineSpacing");
    }
    if (const RuntimeValue* alignment = findValue(data, "lineAlignment")) {
        result->lineAlignment = lineAlignment(
            requireString(*alignment, textConfigKey + ".lineAlignment"),
            textConfigKey + ".lineAlignment");
    }
    if (const RuntimeValue* outline = findValue(data, "outline")) {
        const RuntimeValue::Map& map =
            requireMap(*outline, textConfigKey + ".outline");
        if (const RuntimeValue* color = findValue(map, "color")) {
            result->outline.color =
                requireColor(*color, textConfigKey + ".outline.color");
        }
        if (const RuntimeValue* thickness = findValue(map, "thickness")) {
            result->outline.thickness =
                requireFloat(*thickness, textConfigKey + ".outline.thickness");
        }
    }
    applyGlow(result->glow, findValue(data, "glow"), textConfigKey + ".glow");
    applyGradient(result->gradient, findValue(data, "gradient"),
                  textConfigKey + ".gradient");
    return result;
}

std::shared_ptr<PlainTextConfig> plainTextControlConfig(
    const RuntimeValue::Map& properties) {
    std::shared_ptr<PlainTextConfig> result =
        plainTextConfig(stringProperty(properties, "textConfig"));
    const RuntimeValue* outlineColor = findValue(properties, "outlineColor");
    if (outlineColor != nullptr && !outlineColor->isNil()) {
        result->outline.color = requireColor(*outlineColor, "outlineColor");
    }
    const RuntimeValue* outlineThickness =
        findValue(properties, "outlineThickness");
    if (outlineThickness != nullptr && !outlineThickness->isNil()) {
        result->outline.thickness =
            requireFloat(*outlineThickness, "outlineThickness");
    }
    return result;
}

std::shared_ptr<TextStyle> richTextStyle(const RuntimeValue& value,
                                         const std::string& source) {
    const RuntimeValue::Map& map = requireMap(value, source);
    std::shared_ptr<TextStyle> result = std::make_shared<TextStyle>();
    if (const RuntimeValue* characterSize = findValue(map, "characterSize")) {
        result->characterSize =
            requireUnsigned(*characterSize, source + ".characterSize");
    }
    if (const RuntimeValue* style = findValue(map, "style")) {
        const RuntimeValue::Map& flags = requireMap(*style, source + ".style");
        if (const RuntimeValue* bold = findValue(flags, "bold")) {
            result->bold = requireBool(*bold, source + ".style.bold");
        }
        if (const RuntimeValue* italic = findValue(flags, "italic")) {
            result->italic = requireBool(*italic, source + ".style.italic");
        }
        if (const RuntimeValue* underlined = findValue(flags, "underlined")) {
            result->underlined =
                requireBool(*underlined, source + ".style.underlined");
        }
        if (const RuntimeValue* strike = findValue(flags, "strikeThrough")) {
            result->strikeThrough =
                requireBool(*strike, source + ".style.strikeThrough");
        }
    }
    if (const RuntimeValue* fillColor = findValue(map, "fillColor")) {
        result->fillColor = requireColor(*fillColor, source + ".fillColor");
    }
    if (const RuntimeValue* letterSpacing = findValue(map, "letterSpacing")) {
        result->letterSpacing =
            requireFloat(*letterSpacing, source + ".letterSpacing");
    }
    if (const RuntimeValue* lineSpacing = findValue(map, "lineSpacing")) {
        result->lineSpacing =
            requireFloat(*lineSpacing, source + ".lineSpacing");
    }
    if (const RuntimeValue* outline = findValue(map, "outline")) {
        const RuntimeValue::Map& outlineMap =
            requireMap(*outline, source + ".outline");
        if (const RuntimeValue* color = findValue(outlineMap, "color")) {
            result->outlineColor =
                requireColor(*color, source + ".outline.color");
        }
        if (const RuntimeValue* thickness =
                findValue(outlineMap, "thickness")) {
            result->outlineThickness =
                requireFloat(*thickness, source + ".outline.thickness");
        }
    }
    return result;
}

std::shared_ptr<RichTextConfig> richTextConfig(
    const std::string& textConfigKey) {
    std::shared_ptr<RichTextConfig> result = std::make_shared<RichTextConfig>();
    if (textConfigKey.empty()) {
        result->name = "UI Asset Default";
        result->font = loadFont({}, result->name);
        return result;
    }

    const RuntimeValue::Map data = loadTextConfigData(textConfigKey);
    const RuntimeValue* type = findValue(data, "type");
    if (type == nullptr ||
        requireString(*type, textConfigKey + ".type") != "richTextConfig") {
        throw std::invalid_argument("UI text config is not rich text: " +
                                    textConfigKey);
    }
    result->name = stringProperty(data, "name", textConfigKey);
    result->font = loadFont(stringProperty(data, "font"), textConfigKey);
    if (const RuntimeValue* alignment = findValue(data, "lineAlignment")) {
        result->lineAlignment = lineAlignment(
            requireString(*alignment, textConfigKey + ".lineAlignment"),
            textConfigKey + ".lineAlignment");
    }
    if (const RuntimeValue* defaultStyle = findValue(data, "defaultStyle")) {
        result->defaultStyle =
            richTextStyle(*defaultStyle, textConfigKey + ".defaultStyle");
    }
    if (const RuntimeValue* styleOrder = findValue(data, "styleOrder")) {
        const RuntimeValue::Array& order =
            requireArray(*styleOrder, textConfigKey + ".styleOrder");
        result->styleOrder.reserve(order.size());
        for (std::size_t index = 0; index < order.size(); ++index) {
            result->styleOrder.push_back(requireString(
                order[index],
                textConfigKey + ".styleOrder[" + std::to_string(index) + "]"));
        }
    }
    if (const RuntimeValue* styles = findValue(data, "styles")) {
        const RuntimeValue::Map& styleMap =
            requireMap(*styles, textConfigKey + ".styles");
        for (const std::string& name : result->styleOrder) {
            const RuntimeValue* style = findValue(styleMap, name);
            if (style == nullptr) {
                throw std::invalid_argument(textConfigKey +
                                            ".styles is missing " + name);
            }
            result->styles.emplace(
                name, richTextStyle(*style, textConfigKey + ".styles." + name));
        }
        if (styleMap.size() != result->styles.size()) {
            throw std::invalid_argument(
                textConfigKey +
                ".styles contains entries not listed by styleOrder");
        }
    }
    applyGlow(result->glow, findValue(data, "glow"), textConfigKey + ".glow");
    applyGradient(result->gradient, findValue(data, "gradient"),
                  textConfigKey + ".gradient");
    return result;
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
