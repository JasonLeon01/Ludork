#include "TextConfigCodec.hpp"

#include <Runtime/AssetStore.hpp>
#include <Runtime/RuntimeValueReader.hpp>
#include <UI/UIState.hpp>
#include <UI/UiVector4CurveResource.hpp>
#include <Runtime/Json.hpp>

#include <Utf8Path.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace ludork::engine::text_config {

namespace {

using ludork::runtime::value_reader::findValue;
using ludork::runtime::value_reader::requireArray;
using ludork::runtime::value_reader::requireBool;
using ludork::runtime::value_reader::requireMap;
using ludork::runtime::value_reader::requireNumber;
using ludork::runtime::value_reader::requireString;
using ludork::runtime::value_reader::requireValue;

struct FontResource {
    std::unique_ptr<ludork::runtime::AssetInputStream> stream;
    sf::Font font;
};

[[noreturn]] void configError(const std::string& source,
                              const std::string& message) {
    throw std::invalid_argument(message + " in text config " + source);
}

void onlyFields(const RuntimeValue::Map& value,
                std::initializer_list<std::string_view> fields,
                const std::string& source) {
    for (const auto& [key, _] : value) {
        if (std::find(fields.begin(), fields.end(), key) == fields.end()) {
            configError(source, "Unknown field " + key);
        }
    }
}

const RuntimeValue& required(const RuntimeValue::Map& value,
                             const std::string& field,
                             const std::string& source) {
    const RuntimeValue* result = findValue(value, field);
    if (result == nullptr) {
        configError(source, "Missing " + field);
    }
    return *result;
}

std::string stringValue(const RuntimeValue& value, const std::string& source,
                        bool allowEmpty) {
    const std::string& result = requireString(value, source);
    if (!allowEmpty && result.empty()) {
        configError(source, "Expected a non-empty string");
    }
    return result;
}

double numberValue(const RuntimeValue& value, const std::string& source,
                   double minimum, double maximum) {
    const double result = requireNumber(value, source);
    if (result < minimum) {
        configError(source, "Expected a number greater than or equal to " +
                                std::to_string(minimum));
    }
    if (result > maximum) {
        configError(source, "Expected a number less than or equal to " +
                                std::to_string(maximum));
    }
    return result;
}

std::int64_t integerValue(const RuntimeValue& value, const std::string& source,
                          std::int64_t minimum, std::int64_t maximum) {
    const double result = requireNumber(value, source);
    if (std::floor(result) != result || result < static_cast<double>(minimum) ||
        result > static_cast<double>(maximum)) {
        configError(source, "Expected an integer between " +
                                std::to_string(minimum) + " and " +
                                std::to_string(maximum));
    }
    return static_cast<std::int64_t>(result);
}

sf::Color colorValue(const RuntimeValue& value, const std::string& source) {
    const RuntimeValue::Array& channels = requireArray(value, source);
    if (channels.size() != 3 && channels.size() != 4) {
        configError(source, "Expected three or four colour channels");
    }
    const auto channel = [&](std::size_t index) {
        return static_cast<std::uint8_t>(integerValue(
            channels[index], source + "[" + std::to_string(index + 1) + "]", 0,
            255));
    };
    return {channel(0), channel(1), channel(2),
            channels.size() == 4 ? channel(3) : std::uint8_t{255}};
}

struct StyleFlags {
    bool bold = false;
    bool italic = false;
    bool underlined = false;
    bool strikeThrough = false;
};

StyleFlags styleFlags(const RuntimeValue& value, const std::string& source,
                      bool requireAll) {
    const RuntimeValue::Map& flags = requireMap(value, source);
    onlyFields(flags, {"bold", "italic", "underlined", "strikeThrough"},
               source);
    StyleFlags result;
    const auto read = [&](const std::string& name, bool& target) {
        const RuntimeValue* setting = findValue(flags, name);
        if (setting == nullptr) {
            if (requireAll) {
                configError(source, "Missing " + name);
            }
            return false;
        }
        target = requireBool(*setting, source + "." + name);
        return true;
    };
    read("bold", result.bold);
    read("italic", result.italic);
    read("underlined", result.underlined);
    read("strikeThrough", result.strikeThrough);
    return result;
}

std::uint32_t plainStyle(const RuntimeValue& value, const std::string& source) {
    const StyleFlags flags = styleFlags(value, source, true);
    std::uint32_t result = sf::Text::Regular;
    if (flags.bold) {
        result |= sf::Text::Bold;
    }
    if (flags.italic) {
        result |= sf::Text::Italic;
    }
    if (flags.underlined) {
        result |= sf::Text::Underlined;
    }
    if (flags.strikeThrough) {
        result |= sf::Text::StrikeThrough;
    }
    return result;
}

TextOutlineConfig outlineValue(const RuntimeValue& value,
                               const std::string& source) {
    const RuntimeValue::Map& map = requireMap(value, source);
    onlyFields(map, {"color", "thickness"}, source);
    return {
        .color = colorValue(required(map, "color", source), source + ".color"),
        .thickness =
            static_cast<float>(numberValue(required(map, "thickness", source),
                                           source + ".thickness", 0.0, 32.0)),
    };
}

TextGlowConfig glowValue(const RuntimeValue& value, const std::string& source) {
    const RuntimeValue::Map& map = requireMap(value, source);
    onlyFields(map, {"enabled", "color", "radius", "intensity"}, source);
    return {
        .enabled =
            requireBool(required(map, "enabled", source), source + ".enabled"),
        .color = colorValue(required(map, "color", source), source + ".color"),
        .radius = static_cast<float>(numberValue(
            required(map, "radius", source), source + ".radius", 0.0, 64.0)),
        .intensity =
            static_cast<float>(numberValue(required(map, "intensity", source),
                                           source + ".intensity", 0.0, 1.0)),
    };
}

TextGradientConfig gradientValue(const RuntimeValue& value,
                                 const std::string& source) {
    const RuntimeValue::Map& map = requireMap(value, source);
    onlyFields(map, {"enabled", "direction", "curve"}, source);
    TextGradientConfig result;
    result.enabled =
        requireBool(required(map, "enabled", source), source + ".enabled");
    result.direction = stringValue(required(map, "direction", source),
                                   source + ".direction", false);
    if (result.direction != "horizontal" && result.direction != "vertical") {
        configError(source + ".direction",
                    "Invalid gradient direction " + result.direction);
    }
    const std::string curveName =
        stringValue(required(map, "curve", source), source + ".curve", true);
    if (!curveName.empty()) {
        result.curve = loadUiVector4CurveResource(curveName);
    } else if (result.enabled) {
        configError(source + ".curve", "Enabled gradient requires a curve");
    }
    return result;
}

std::shared_ptr<TextStyle> richStyle(const RuntimeValue& value,
                                     const std::string& source,
                                     bool requireAll) {
    const RuntimeValue::Map& map = requireMap(value, source);
    onlyFields(map,
               {"characterSize", "style", "fillColor", "letterSpacing",
                "lineSpacing", "outline"},
               source);
    std::shared_ptr<TextStyle> result = std::make_shared<TextStyle>();
    if (const RuntimeValue* characterSize = findValue(map, "characterSize")) {
        result->characterSize = static_cast<unsigned int>(
            integerValue(*characterSize, source + ".characterSize", 1, 512));
    } else if (requireAll) {
        configError(source, "Missing characterSize");
    }
    if (const RuntimeValue* style = findValue(map, "style")) {
        const RuntimeValue::Map& flags = requireMap(*style, source + ".style");
        onlyFields(flags, {"bold", "italic", "underlined", "strikeThrough"},
                   source + ".style");
        const auto assign = [&](const std::string& name,
                                std::optional<bool>& target) {
            const RuntimeValue* setting = findValue(flags, name);
            if (setting != nullptr) {
                target = requireBool(*setting, source + ".style." + name);
            } else if (requireAll) {
                configError(source + ".style", "Missing " + name);
            }
        };
        assign("bold", result->bold);
        assign("italic", result->italic);
        assign("underlined", result->underlined);
        assign("strikeThrough", result->strikeThrough);
    } else if (requireAll) {
        configError(source, "Missing style");
    }
    if (const RuntimeValue* fillColor = findValue(map, "fillColor")) {
        result->fillColor = colorValue(*fillColor, source + ".fillColor");
    } else if (requireAll) {
        configError(source, "Missing fillColor");
    }
    if (const RuntimeValue* spacing = findValue(map, "letterSpacing")) {
        result->letterSpacing = static_cast<float>(
            numberValue(*spacing, source + ".letterSpacing", 0.1, 10.0));
    } else if (requireAll) {
        configError(source, "Missing letterSpacing");
    }
    if (const RuntimeValue* spacing = findValue(map, "lineSpacing")) {
        result->lineSpacing = static_cast<float>(
            numberValue(*spacing, source + ".lineSpacing", 0.1, 10.0));
    } else if (requireAll) {
        configError(source, "Missing lineSpacing");
    }
    if (const RuntimeValue* outline = findValue(map, "outline")) {
        const RuntimeValue::Map& outlineMap =
            requireMap(*outline, source + ".outline");
        onlyFields(outlineMap, {"color", "thickness"}, source + ".outline");
        if (const RuntimeValue* color = findValue(outlineMap, "color")) {
            result->outlineColor =
                colorValue(*color, source + ".outline.color");
        } else if (requireAll) {
            configError(source + ".outline", "Missing color");
        }
        if (const RuntimeValue* thickness =
                findValue(outlineMap, "thickness")) {
            result->outlineThickness = static_cast<float>(numberValue(
                *thickness, source + ".outline.thickness", 0.0, 32.0));
        } else if (requireAll) {
            configError(source + ".outline", "Missing thickness");
        }
    } else if (requireAll) {
        configError(source, "Missing outline");
    }
    return result;
}

std::filesystem::path safeRelativePath(const std::string& value) {
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
    return relative;
}

RuntimeValue::Map loadConfigData(const std::string& textConfigKey) {
    std::filesystem::path relative = safeRelativePath(textConfigKey);
    if (relative.extension().empty()) {
        relative += ".json";
    }
    const std::filesystem::path path =
        std::filesystem::path(".") / "Data" / "TextConfigs" / relative;
    return requireMap(RuntimeValue(getJSONData(path)),
                      ludork::standard::pathToUtf8(path));
}

}  // namespace

std::shared_ptr<sf::Font> loadFont(const std::string& fontKey,
                                   const std::string& source) {
    if (fontKey.empty()) {
        if (defaultFont == nullptr) {
            throw std::runtime_error(
                "Default font is unavailable for UI text config " + source);
        }
        return defaultFont;
    }
    std::shared_ptr<FontResource> owner = std::make_shared<FontResource>();
    owner->stream = ludork::runtime::assetStore().open(fontKey);
    if (!owner->font.openFromStream(*owner->stream)) {
        throw std::runtime_error("Failed to load UI font: " + fontKey);
    }
    return std::shared_ptr<sf::Font>(owner, &owner->font);
}

sf::Text::LineAlignment parseLineAlignment(const std::string& value,
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
    configError(source, "Invalid line alignment " + value);
}

std::shared_ptr<PlainTextConfig> buildPlain(const RuntimeValue::Map& data,
                                            const std::string& sourceName) {
    onlyFields(data,
               {"type", "name", "font", "characterSize", "style", "slantAngle",
                "fillColor", "letterSpacing", "lineSpacing", "lineAlignment",
                "outline", "glow", "gradient"},
               sourceName);
    const std::string type = stringValue(required(data, "type", sourceName),
                                         sourceName + ".type", false);
    if (type != "plainTextConfig") {
        configError(sourceName + ".type", "Expected plainTextConfig");
    }
    std::shared_ptr<PlainTextConfig> result =
        std::make_shared<PlainTextConfig>();
    result->type = type;
    result->name = stringValue(required(data, "name", sourceName),
                               sourceName + ".name", true);
    result->font = loadFont(stringValue(required(data, "font", sourceName),
                                        sourceName + ".font", true),
                            sourceName + ".font");
    result->characterSize = static_cast<unsigned int>(
        integerValue(required(data, "characterSize", sourceName),
                     sourceName + ".characterSize", 1, 512));
    result->style =
        plainStyle(required(data, "style", sourceName), sourceName + ".style");
    if (const RuntimeValue* angle = findValue(data, "slantAngle")) {
        result->slantAngle = static_cast<float>(
            numberValue(*angle, sourceName + ".slantAngle", -45.0, 45.0));
    }
    result->fillColor = colorValue(required(data, "fillColor", sourceName),
                                   sourceName + ".fillColor");
    result->letterSpacing = static_cast<float>(
        numberValue(required(data, "letterSpacing", sourceName),
                    sourceName + ".letterSpacing", 0.1, 10.0));
    result->lineSpacing = static_cast<float>(
        numberValue(required(data, "lineSpacing", sourceName),
                    sourceName + ".lineSpacing", 0.1, 10.0));
    result->lineAlignment = parseLineAlignment(
        stringValue(required(data, "lineAlignment", sourceName),
                    sourceName + ".lineAlignment", false),
        sourceName + ".lineAlignment");
    result->outline = outlineValue(required(data, "outline", sourceName),
                                   sourceName + ".outline");
    result->glow =
        glowValue(required(data, "glow", sourceName), sourceName + ".glow");
    result->gradient = gradientValue(required(data, "gradient", sourceName),
                                     sourceName + ".gradient");
    return result;
}

std::shared_ptr<RichTextConfig> buildRich(const RuntimeValue::Map& data,
                                          const std::string& sourceName) {
    onlyFields(data,
               {"type", "name", "font", "lineAlignment", "defaultStyle",
                "styleOrder", "styles", "glow", "gradient"},
               sourceName);
    const std::string type = stringValue(required(data, "type", sourceName),
                                         sourceName + ".type", false);
    if (type != "richTextConfig") {
        configError(sourceName + ".type", "Expected richTextConfig");
    }
    std::shared_ptr<RichTextConfig> result = std::make_shared<RichTextConfig>();
    result->type = type;
    result->name = stringValue(required(data, "name", sourceName),
                               sourceName + ".name", true);
    result->font = loadFont(stringValue(required(data, "font", sourceName),
                                        sourceName + ".font", true),
                            sourceName + ".font");
    result->lineAlignment = parseLineAlignment(
        stringValue(required(data, "lineAlignment", sourceName),
                    sourceName + ".lineAlignment", false),
        sourceName + ".lineAlignment");
    result->defaultStyle = richStyle(required(data, "defaultStyle", sourceName),
                                     sourceName + ".defaultStyle", true);

    const RuntimeValue::Array& order = requireArray(
        required(data, "styleOrder", sourceName), sourceName + ".styleOrder");
    const RuntimeValue::Map& styles = requireMap(
        required(data, "styles", sourceName), sourceName + ".styles");
    std::unordered_set<std::string> orderedStyles;
    result->styleOrder.reserve(order.size());
    for (std::size_t index = 0; index < order.size(); ++index) {
        const std::string styleName = stringValue(
            order[index],
            sourceName + ".styleOrder[" + std::to_string(index + 1) + "]",
            false);
        if (styleName == "default" ||
            styleName.find('#') != std::string::npos) {
            configError(sourceName + ".styleOrder",
                        "Reserved rich text style name " + styleName);
        }
        if (!orderedStyles.insert(styleName).second) {
            configError(sourceName + ".styleOrder",
                        "Duplicate rich text style " + styleName);
        }
        const RuntimeValue* style = findValue(styles, styleName);
        if (style == nullptr) {
            configError(sourceName + ".styles",
                        "Missing rich text style " + styleName);
        }
        result->styleOrder.push_back(styleName);
        result->styles.emplace(
            styleName,
            richStyle(*style, sourceName + ".styles." + styleName, false));
    }
    for (const auto& [styleName, _] : styles) {
        if (!orderedStyles.contains(styleName)) {
            configError(sourceName + ".styles", "Rich text style " + styleName +
                                                    " is not listed in "
                                                    "styleOrder");
        }
    }
    result->glow =
        glowValue(required(data, "glow", sourceName), sourceName + ".glow");
    result->gradient = gradientValue(required(data, "gradient", sourceName),
                                     sourceName + ".gradient");
    return result;
}

std::shared_ptr<PlainTextConfig> loadPlain(const std::string& textConfigKey) {
    return buildPlain(loadConfigData(textConfigKey), textConfigKey);
}

std::shared_ptr<RichTextConfig> loadRich(const std::string& textConfigKey) {
    return buildRich(loadConfigData(textConfigKey), textConfigKey);
}

}  // namespace ludork::engine::text_config
