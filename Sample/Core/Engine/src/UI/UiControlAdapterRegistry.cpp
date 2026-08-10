#include <UI/UiControlAdapterRegistry.hpp>

#include <Curve.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <UI/Button.hpp>
#include <UI/Canvas.hpp>
#include <UI/CheckBox.hpp>
#include <UI/DropBox.hpp>
#include <UI/FunctionalUI.hpp>
#include <UI/Image.hpp>
#include <UI/ListView.hpp>
#include <UI/ProgressBar.hpp>
#include <UI/Rect.hpp>
#include <UI/Slider.hpp>
#include <UI/SolidRect.hpp>
#include <UI/Text.hpp>
#include <UI/UIState.hpp>
#include <UI/UiVector4CurveResource.hpp>
#include <UI/Window.hpp>
#include <Utils/File.hpp>

#include <Utf8Path.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <mutex>
#include <optional>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr std::array<std::uint32_t, 64> sha256Constants = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
    0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
    0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
    0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
    0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
    0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

std::string sha256(std::string_view value) {
    std::vector<std::uint8_t> message(value.begin(), value.end());
    const std::uint64_t bitLength =
        static_cast<std::uint64_t>(message.size()) * 8u;
    message.push_back(0x80u);
    while (message.size() % 64u != 56u) {
        message.push_back(0u);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(
            static_cast<std::uint8_t>((bitLength >> shift) & 0xffu));
    }

    std::array<std::uint32_t, 8> hash = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    for (std::size_t block = 0; block < message.size(); block += 64u) {
        std::array<std::uint32_t, 64> schedule{};
        for (std::size_t index = 0; index < 16u; ++index) {
            const std::size_t offset = block + index * 4u;
            schedule[index] =
                (static_cast<std::uint32_t>(message[offset]) << 24u) |
                (static_cast<std::uint32_t>(message[offset + 1u]) << 16u) |
                (static_cast<std::uint32_t>(message[offset + 2u]) << 8u) |
                static_cast<std::uint32_t>(message[offset + 3u]);
        }
        for (std::size_t index = 16u; index < schedule.size(); ++index) {
            const std::uint32_t first = std::rotr(schedule[index - 15u], 7) ^
                                        std::rotr(schedule[index - 15u], 18) ^
                                        (schedule[index - 15u] >> 3u);
            const std::uint32_t second = std::rotr(schedule[index - 2u], 17) ^
                                         std::rotr(schedule[index - 2u], 19) ^
                                         (schedule[index - 2u] >> 10u);
            schedule[index] =
                schedule[index - 16u] + first + schedule[index - 7u] + second;
        }

        std::array<std::uint32_t, 8> state = hash;
        for (std::size_t index = 0; index < schedule.size(); ++index) {
            const std::uint32_t sum1 = std::rotr(state[4], 6) ^
                                       std::rotr(state[4], 11) ^
                                       std::rotr(state[4], 25);
            const std::uint32_t choice =
                (state[4] & state[5]) ^ (~state[4] & state[6]);
            const std::uint32_t temporary1 = state[7] + sum1 + choice +
                                             sha256Constants[index] +
                                             schedule[index];
            const std::uint32_t sum0 = std::rotr(state[0], 2) ^
                                       std::rotr(state[0], 13) ^
                                       std::rotr(state[0], 22);
            const std::uint32_t majority = (state[0] & state[1]) ^
                                           (state[0] & state[2]) ^
                                           (state[1] & state[2]);
            const std::uint32_t temporary2 = sum0 + majority;
            state[7] = state[6];
            state[6] = state[5];
            state[5] = state[4];
            state[4] = state[3] + temporary1;
            state[3] = state[2];
            state[2] = state[1];
            state[1] = state[0];
            state[0] = temporary1 + temporary2;
        }
        for (std::size_t index = 0; index < hash.size(); ++index) {
            hash[index] += state[index];
        }
    }

    constexpr std::string_view digits = "0123456789abcdef";
    std::string result;
    result.reserve(hash.size() * 8u);
    for (const std::uint32_t word : hash) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            result.push_back(digits[(word >> shift) & 0x0fu]);
        }
    }
    return result;
}

std::string_view childPolicyName(UiChildPolicy policy) {
    switch (policy) {
        case UiChildPolicy::None:
            return "none";
        case UiChildPolicy::Single:
            return "single";
        case UiChildPolicy::Multiple:
            return "multiple";
    }
    throw std::logic_error("Unknown UI child policy");
}

std::string_view slotTypeName(UiControlSlotType slotType) {
    switch (slotType) {
        case UiControlSlotType::None:
            return {};
        case UiControlSlotType::Canvas:
            return "canvas";
        case UiControlSlotType::List:
            return "list";
    }
    throw std::logic_error("Unknown UI Slot type");
}

std::string adapterFingerprintSource() {
    std::vector<const UiControlAdapterDescriptor*> descriptors;
    descriptors.reserve(uiControlAdapterDescriptorTable.size());
    for (const UiControlAdapterDescriptor& descriptor :
         uiControlAdapterDescriptorTable) {
        descriptors.push_back(&descriptor);
    }
    std::sort(descriptors.begin(), descriptors.end(),
              [](const UiControlAdapterDescriptor* left,
                 const UiControlAdapterDescriptor* right) {
                  return left->controlId < right->controlId;
              });

    std::string source;
    for (const UiControlAdapterDescriptor* descriptor : descriptors) {
        source.append(descriptor->controlId);
        source.push_back('|');
        source.append(descriptor->adapter);
        source.push_back('|');
        source.append(childPolicyName(descriptor->childPolicy));
        source.push_back('|');
        source.append(slotTypeName(descriptor->slotType));
        source.push_back('|');
        for (const UiControlPropertyDescriptor& property :
             descriptor->properties) {
            if (property.editorOnly) {
                continue;
            }
            source.append(property.id);
            source.push_back(':');
            source.append(property.type);
            source.push_back(':');
            source.append(property.required ? "true" : "false");
            source.push_back(';');
        }
        source.push_back('\n');
    }
    return source;
}

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
                           const std::string& fallback = {}) {
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
    return requireMap(getJSONData(path),
                      ludork::standard::pathToUtf8(path));
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

template <typename T>
T& requireControlType(ControlBase& control, const std::string& controlId) {
    T* typed = dynamic_cast<T*>(&control);
    if (typed == nullptr) {
        throw std::logic_error("UI adapter received the wrong control type: " +
                               controlId);
    }
    return *typed;
}

}  // namespace

std::span<const UiControlAdapterDescriptor> uiControlAdapterDescriptors() {
    return uiControlAdapterDescriptorTable;
}

std::string_view uiControlAdapterFingerprint() {
    static const std::string fingerprint = sha256(adapterFingerprintSource());
    return fingerprint;
}

void clearUiControlAdapterResourceCache() noexcept {
    PlaceholderTextureCache& cache = placeholderTextureCache();
    std::lock_guard<std::mutex> lock(cache.mutex);
    cache.texture.reset();
}

const UiControlAdapterRegistry& UiControlAdapterRegistry::instance() {
    static UiControlAdapterRegistry registry;
    return registry;
}

bool UiControlAdapterRegistry::contains(const std::string& controlId) const {
    return adapters_.contains(controlId);
}

UiChildPolicy UiControlAdapterRegistry::childPolicy(
    const std::string& controlId) const {
    return requireAdapter(controlId).childPolicy;
}

UiControlSlotType UiControlAdapterRegistry::slotType(
    const std::string& controlId) const {
    return requireAdapter(controlId).slotType;
}

bool UiControlAdapterRegistry::supportsProperty(
    const std::string& controlId, const std::string& propertyId) const {
    return requireAdapter(controlId).properties.contains(propertyId);
}

std::shared_ptr<ControlBase> UiControlAdapterRegistry::create(
    const std::string& controlId, const RuntimeValue::Map& properties) const {
    return requireAdapter(controlId).factory(properties);
}

void UiControlAdapterRegistry::setProperty(const std::string& controlId,
                                           ControlBase& control,
                                           const std::string& propertyId,
                                           const RuntimeValue& value) const {
    const Adapter& adapter = requireAdapter(controlId);
    if (!adapter.properties.contains(propertyId)) {
        throw std::invalid_argument("Unknown property " + propertyId +
                                    " for UI control " + controlId);
    }
    adapter.setter(control, propertyId, value);
}

sf::Vector2f UiControlAdapterRegistry::measure(
    const ControlBase& control) const {
    return control.getSize();
}

void UiControlAdapterRegistry::arrange(const std::string& controlId,
                                       ControlBase& control,
                                       const sf::Vector2f& size,
                                       const sf::Vector2f& renderScale) const {
    requireAdapter(controlId).arranger(control, size, renderScale);
}

void UiControlAdapterRegistry::attachChildren(
    const std::string& controlId, ControlBase& control,
    const std::vector<std::shared_ptr<ControlBase>>& children) const {
    const Adapter& adapter = requireAdapter(controlId);
    if (!adapter.childAttacher) {
        throw std::logic_error(
            "UI adapter does not implement child attachment: " + controlId);
    }
    adapter.childAttacher(control, children);
}

void UiControlAdapterRegistry::reflowChildren(const std::string& controlId,
                                              ControlBase& control) const {
    const Adapter& adapter = requireAdapter(controlId);
    if (adapter.childReflow) {
        adapter.childReflow(control);
    }
}

UiControlAdapterRegistry::UiControlAdapterRegistry() {
    Adapter canvas;
    canvas.factory = [](const RuntimeValue::Map& properties) {
        const sf::Vector2u size =
            vector2uProperty(properties, "size", {100u, 100u});
        return std::make_shared<Canvas>(sf::IntRect(
            {0, 0}, {static_cast<int>(size.x), static_cast<int>(size.y)}));
    };
    canvas.setter = [](ControlBase& control, const std::string& propertyId,
                       const RuntimeValue& value) {
        Canvas& canvas =
            requireControlType<Canvas>(control, "Engine.Canvas");
        if (propertyId == "size") {
            canvas.resize(requireVector2u(value, "size"));
            return;
        }
        throw std::invalid_argument("Unknown Canvas property " + propertyId);
    };
    canvas.arranger = [](ControlBase& control, const sf::Vector2f& size,
                         const sf::Vector2f& renderScale) {
        Canvas& canvas =
            requireControlType<Canvas>(control, "Engine.Canvas");
        canvas.resize(
            {static_cast<unsigned int>(std::max(0.0f, std::round(size.x))),
             static_cast<unsigned int>(std::max(0.0f, std::round(size.y)))});
        canvas.setScale(renderScale);
    };
    canvas.childAttacher =
        [](ControlBase& control,
           const std::vector<std::shared_ptr<ControlBase>>& children) {
            Canvas& canvas =
                requireControlType<Canvas>(control, "Engine.Canvas");
            for (const std::shared_ptr<ControlBase>& child : children) {
                canvas.addChild(child);
            }
        };
    registerAdapter<CanvasUiControlAdapterTag>(std::move(canvas));

    Adapter listView;
    listView.factory = [](const RuntimeValue::Map& properties) {
        const sf::Vector2f size =
            vector2fProperty(properties, "size", {100.0f, 100.0f});
        const int defaultItemHeight =
            intProperty(properties, "defaultItemHeight", 32);
        const bool fixItemHeight =
            boolProperty(properties, "fixItemHeight", false);
        const int columns = intProperty(properties, "columns", 1);
        return std::make_shared<ListView>(
            sf::IntRect({0, 0}, {static_cast<int>(std::round(size.x)),
                                 static_cast<int>(std::round(size.y))}),
            defaultItemHeight, fixItemHeight, columns);
    };
    listView.setter = [](ControlBase& control, const std::string& propertyId,
                         const RuntimeValue& value) {
        ListView& list =
            requireControlType<ListView>(control, "Engine.ListView");
        if (propertyId == "size") {
            list.setSizeVector2f(requireVector2f(value, "size"));
            return;
        }
        if (propertyId == "columns") {
            list.setColumns(requireInt(value, "columns"));
            return;
        }
        throw std::invalid_argument(
            propertyId + " is a construction-only ListView property");
    };
    listView.arranger = [](ControlBase& control, const sf::Vector2f& size,
                           const sf::Vector2f& renderScale) {
        ListView& list =
            requireControlType<ListView>(control, "Engine.ListView");
        list.setSizeVector2f({std::max(0.0f, size.x), std::max(0.0f, size.y)});
        list.setScale(renderScale);
        list.invalidatePositions();
    };
    listView.childAttacher =
        [](ControlBase& control,
           const std::vector<std::shared_ptr<ControlBase>>& children) {
            ListView& list =
                requireControlType<ListView>(control, "Engine.ListView");
            for (const std::shared_ptr<ControlBase>& child : children) {
                list.addChild(child);
            }
        };
    listView.childReflow = [](ControlBase& control) {
        ListView& list =
            requireControlType<ListView>(control, "Engine.ListView");
        list.invalidatePositions();
        list.applyPositions();
    };
    registerAdapter<ListViewUiControlAdapterTag>(std::move(listView));

    Adapter solidRect;
    solidRect.factory = [](const RuntimeValue::Map& properties) {
        return std::make_shared<SolidRect>(
            vector2fProperty(properties, "size", {100.0f, 32.0f}),
            colorProperty(properties, "fillColor", sf::Color::White),
            colorProperty(properties, "outlineColor", sf::Color::Transparent),
            floatProperty(properties, "outlineThickness", 0.0f));
    };
    solidRect.setter = [](ControlBase& control, const std::string& propertyId,
                          const RuntimeValue& value) {
        SolidRect& rect =
            requireControlType<SolidRect>(control, "Engine.SolidRect");
        if (propertyId == "size") {
            rect.setSize(requireVector2f(value, "size"));
        } else if (propertyId == "fillColor") {
            rect.setFillColor(requireColor(value, "fillColor"));
        } else if (propertyId == "outlineColor") {
            rect.setOutlineColor(requireColor(value, "outlineColor"));
        } else if (propertyId == "outlineThickness") {
            rect.setOutlineThickness(requireFloat(value, "outlineThickness"));
        } else {
            throw std::invalid_argument("Unknown SolidRect property " +
                                        propertyId);
        }
    };
    solidRect.arranger = [](ControlBase& control, const sf::Vector2f& size,
                            const sf::Vector2f& renderScale) {
        SolidRect& rect =
            requireControlType<SolidRect>(control, "Engine.SolidRect");
        rect.setSize({std::max(0.0f, size.x), std::max(0.0f, size.y)});
        rect.setScale(renderScale);
    };
    registerAdapter<SolidRectUiControlAdapterTag>(std::move(solidRect));

    Adapter progressBar;
    progressBar.factory = [](const RuntimeValue::Map& properties) {
        return std::make_shared<ProgressBar>(
            vector2fProperty(properties, "size", {100.0f, 12.0f}),
            floatProperty(properties, "progress", 0.0f),
            colorProperty(properties, "backgroundColor",
                          sf::Color(255, 255, 255, 64)),
            colorProperty(properties, "fillColor", sf::Color::White));
    };
    progressBar.setter = [](ControlBase& control, const std::string& propertyId,
                            const RuntimeValue& value) {
        ProgressBar& progress =
            requireControlType<ProgressBar>(control, "Engine.ProgressBar");
        if (propertyId == "size") {
            progress.resize(requireVector2f(value, "size"));
        } else if (propertyId == "progress") {
            progress.setProgress(requireFloat(value, "progress"));
        } else if (propertyId == "backgroundColor") {
            progress.setBackgroundColor(requireColor(value, "backgroundColor"));
        } else if (propertyId == "fillColor") {
            progress.setFillColor(requireColor(value, "fillColor"));
        } else {
            throw std::invalid_argument("Unknown ProgressBar property " +
                                        propertyId);
        }
    };
    progressBar.arranger = [](ControlBase& control, const sf::Vector2f& size,
                              const sf::Vector2f& renderScale) {
        ProgressBar& progress =
            requireControlType<ProgressBar>(control, "Engine.ProgressBar");
        progress.resize(size);
        progress.setScale(renderScale);
    };
    registerAdapter<ProgressBarUiControlAdapterTag>(std::move(progressBar));

    Adapter image;
    image.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<Image> result = std::make_shared<Image>(
            loadTexture(stringProperty(properties, "texture")),
            optionalIntRectProperty(properties, "textureRect"));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    image.setter = [](ControlBase& control, const std::string& propertyId,
                      const RuntimeValue& value) {
        Image& image = requireControlType<Image>(control, "Engine.Image");
        if (propertyId == "texture") {
            image.setTexture(loadTexture(requireString(value, "texture")),
                             true);
        } else if (propertyId == "textureRect") {
            if (value.isNil()) {
                image.setTextureRect(
                    {{0, 0},
                     {static_cast<int>(image.getTexture().getSize().x),
                      static_cast<int>(image.getTexture().getSize().y)}});
            } else {
                image.setTextureRect(requireIntRect(value, "textureRect"));
            }
        } else if (propertyId == "colour") {
            image.setColour(requireColor(value, "colour"));
        } else {
            throw std::invalid_argument("Unknown Image property " + propertyId);
        }
    };
    image.arranger = [](ControlBase& control, const sf::Vector2f& size,
                        const sf::Vector2f& renderScale) {
        arrangeByScale(control, size, renderScale);
    };
    registerAdapter<ImageUiControlAdapterTag>(std::move(image));

    Adapter functionalImage;
    functionalImage.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<FunctionalImage> result =
            std::make_shared<FunctionalImage>(
                loadTexture(stringProperty(properties, "texture")),
                optionalIntRectProperty(properties, "textureRect"));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    functionalImage.setter = [](ControlBase& control,
                                const std::string& propertyId,
                                const RuntimeValue& value) {
        FunctionalImage& image = requireControlType<FunctionalImage>(
            control, "Engine.FunctionalImage");
        if (propertyId == "texture") {
            image.setTexture(loadTexture(requireString(value, "texture")),
                             true);
        } else if (propertyId == "textureRect") {
            if (value.isNil()) {
                image.setTextureRect(
                    {{0, 0},
                     {static_cast<int>(image.getTexture().getSize().x),
                      static_cast<int>(image.getTexture().getSize().y)}});
            } else {
                image.setTextureRect(requireIntRect(value, "textureRect"));
            }
        } else if (propertyId == "colour") {
            image.setColour(requireColor(value, "colour"));
        } else {
            throw std::invalid_argument("Unknown FunctionalImage property " +
                                        propertyId);
        }
    };
    functionalImage.arranger = [](ControlBase& control,
                                  const sf::Vector2f& size,
                                  const sf::Vector2f& renderScale) {
        arrangeByScale(control, size, renderScale);
    };
    registerAdapter<FunctionalImageUiControlAdapterTag>(
        std::move(functionalImage));

    Adapter button;
    button.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<Button> result = std::make_shared<Button>(
            loadTexture(stringProperty(properties, "texture")),
            optionalIntRectProperty(properties, "textureRect"),
            colorProperty(properties, "hoverColour", sf::Color::White),
            colorProperty(properties, "pressedColour", sf::Color::White));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    button.setter = [](ControlBase& control, const std::string& propertyId,
                       const RuntimeValue& value) {
        Button& button =
            requireControlType<Button>(control, "Engine.Button");
        if (propertyId == "texture") {
            button.setTexture(loadTexture(requireString(value, "texture")),
                              true);
        } else if (propertyId == "textureRect") {
            if (value.isNil()) {
                button.setTextureRect(
                    {{0, 0},
                     {static_cast<int>(button.getTexture().getSize().x),
                      static_cast<int>(button.getTexture().getSize().y)}});
            } else {
                button.setTextureRect(requireIntRect(value, "textureRect"));
            }
        } else if (propertyId == "colour") {
            button.setColour(requireColor(value, "colour"));
        } else if (propertyId == "hoverColour") {
            button.setHoverColour(requireColor(value, "hoverColour"));
        } else if (propertyId == "pressedColour") {
            button.setPressedColour(requireColor(value, "pressedColour"));
        } else {
            throw std::invalid_argument("Unknown Button property " +
                                        propertyId);
        }
    };
    button.arranger = [](ControlBase& control, const sf::Vector2f& size,
                         const sf::Vector2f& renderScale) {
        arrangeByScale(control, size, renderScale);
    };
    registerAdapter<ButtonUiControlAdapterTag>(std::move(button));

    Adapter checkBox;
    checkBox.factory = [](const RuntimeValue::Map& properties) {
        return std::make_shared<CheckBox>(
            vector2fProperty(properties, "size", {32.0f, 32.0f}),
            loadWindowSkin(stringProperty(properties, "windowSkin")),
            plainTextConfig(stringProperty(properties, "textConfig")),
            boolProperty(properties, "checked", false));
    };
    checkBox.setter = [](ControlBase& control, const std::string& propertyId,
                         const RuntimeValue& value) {
        CheckBox& check =
            requireControlType<CheckBox>(control, "Engine.CheckBox");
        if (propertyId == "size") {
            check.resize(requireVector2f(value, "size"));
        } else if (propertyId == "checked") {
            check.setChecked(requireBool(value, "checked"));
        } else if (propertyId == "windowSkin") {
            check.setWindowSkin(
                loadWindowSkin(requireString(value, "windowSkin")));
        } else if (propertyId == "textConfig") {
            check.setTextConfig(
                plainTextConfig(requireString(value, "textConfig")));
        } else {
            throw std::invalid_argument("Unknown CheckBox property " +
                                        propertyId);
        }
    };
    checkBox.arranger = [](ControlBase& control, const sf::Vector2f& size,
                           const sf::Vector2f& renderScale) {
        CheckBox& check =
            requireControlType<CheckBox>(control, "Engine.CheckBox");
        check.resize(size);
        check.setScale(renderScale);
    };
    registerAdapter<CheckBoxUiControlAdapterTag>(std::move(checkBox));

    Adapter slider;
    slider.factory = [](const RuntimeValue::Map& properties) {
        return std::make_shared<Slider>(
            vector2fProperty(properties, "size", {64.0f, 8.0f}),
            loadTexture(stringProperty(properties, "lineTexture")),
            loadTexture(stringProperty(properties, "handleTexture")),
            intProperty(properties, "minValue", 0),
            intProperty(properties, "maxValue", 100),
            intProperty(properties, "value", 0));
    };
    slider.setter = [](ControlBase& control, const std::string& propertyId,
                       const RuntimeValue& value) {
        Slider& range = requireControlType<Slider>(control, "Engine.Slider");
        if (propertyId == "size") {
            range.resize(requireVector2f(value, "size"));
        } else if (propertyId == "value") {
            range.setValue(requireInt(value, "value"));
        } else if (propertyId == "minValue") {
            const std::pair<int, int> limits = range.getRange();
            range.setRange(requireInt(value, "minValue"), limits.second);
        } else if (propertyId == "maxValue") {
            const std::pair<int, int> limits = range.getRange();
            range.setRange(limits.first, requireInt(value, "maxValue"));
        } else if (propertyId == "lineTexture") {
            range.setLineTexture(
                loadTexture(requireString(value, "lineTexture")));
        } else if (propertyId == "handleTexture") {
            range.setHandleTexture(
                loadTexture(requireString(value, "handleTexture")));
        } else {
            throw std::invalid_argument("Unknown Slider property " +
                                        propertyId);
        }
    };
    slider.arranger = [](ControlBase& control, const sf::Vector2f& size,
                         const sf::Vector2f& renderScale) {
        Slider& range = requireControlType<Slider>(control, "Engine.Slider");
        range.resize(size);
        range.setScale(renderScale);
    };
    registerAdapter<SliderUiControlAdapterTag>(std::move(slider));

    Adapter dropBox;
    dropBox.factory = [](const RuntimeValue::Map& properties) {
        const std::string preview = stringProperty(properties, "previewText");
        std::vector<std::string> items;
        if (!preview.empty()) {
            items.push_back(preview);
        }
        return std::make_shared<DropBox>(
            vector2fProperty(properties, "size", {200.0f, 32.0f}),
            loadWindowSkin(stringProperty(properties, "windowSkin")),
            plainTextConfig(stringProperty(properties, "textConfig")),
            std::move(items), 0, false);
    };
    dropBox.setter = [](ControlBase& control, const std::string& propertyId,
                        const RuntimeValue& value) {
        DropBox& field =
            requireControlType<DropBox>(control, "Engine.DropBox");
        if (propertyId == "size") {
            field.setCollapsedSize(requireVector2f(value, "size"));
        } else if (propertyId == "windowSkin") {
            field.setWindowSkin(
                loadWindowSkin(requireString(value, "windowSkin")));
        } else if (propertyId == "textConfig") {
            field.setTextConfig(
                plainTextConfig(requireString(value, "textConfig")));
        } else if (propertyId == "previewText") {
            const std::string& preview = requireString(value, "previewText");
            field.setItems(preview.empty() ? std::vector<std::string>{}
                                           : std::vector<std::string>{preview});
        } else {
            throw std::invalid_argument("Unknown DropBox property " +
                                        propertyId);
        }
    };
    dropBox.arranger = [](ControlBase& control, const sf::Vector2f& size,
                          const sf::Vector2f& renderScale) {
        arrangeByScale(control, size, renderScale);
    };
    dropBox.properties.emplace("previewText");
    registerAdapter<DropBoxUiControlAdapterTag>(std::move(dropBox));

    Adapter window;
    window.factory = [](const RuntimeValue::Map& properties) {
        const sf::Vector2u size =
            vector2uProperty(properties, "size", {160u, 96u});
        return std::make_shared<Window>(
            sf::IntRect({0, 0},
                        {static_cast<int>(size.x), static_cast<int>(size.y)}),
            loadWindowSkin(stringProperty(properties, "windowSkin")),
            boolProperty(properties, "repeated", false));
    };
    window.setter = [](ControlBase& control, const std::string& propertyId,
                       const RuntimeValue& value) {
        Window& window =
            requireControlType<Window>(control, "Engine.Window");
        if (propertyId == "size") {
            window.resize(requireVector2u(value, "size"));
            return;
        }
        throw std::invalid_argument(propertyId +
                                    " is a construction-only Window property");
    };
    window.arranger = [](ControlBase& control, const sf::Vector2f& size,
                         const sf::Vector2f& renderScale) {
        Window& window =
            requireControlType<Window>(control, "Engine.Window");
        window.resize(
            {static_cast<unsigned int>(std::max(0.0f, std::round(size.x))),
             static_cast<unsigned int>(std::max(0.0f, std::round(size.y)))});
        window.setScale(renderScale);
    };
    registerAdapter<WindowUiControlAdapterTag>(std::move(window));

    Adapter rect;
    rect.factory = [](const RuntimeValue::Map& properties) {
        const sf::Vector2f size =
            vector2fProperty(properties, "size", {160.0f, 96.0f});
        const std::string curve = stringProperty(properties, "opacityCurve");
        return std::make_shared<Rect>(
            sf::IntRect({0, 0}, {static_cast<int>(std::round(size.x)),
                                 static_cast<int>(std::round(size.y))}),
            loadWindowSkin(stringProperty(properties, "windowSkin")),
            curve.empty() ? std::nullopt : std::optional<std::string>(curve));
    };
    rect.setter = [](ControlBase& control, const std::string& propertyId,
                     const RuntimeValue& value) {
        Rect& rect = requireControlType<Rect>(control, "Engine.Rect");
        if (propertyId == "size") {
            rect.resize(requireVector2f(value, "size"));
            return;
        }
        throw std::invalid_argument(propertyId +
                                    " is a construction-only Rect property");
    };
    rect.arranger = [](ControlBase& control, const sf::Vector2f& size,
                       const sf::Vector2f& renderScale) {
        Rect& rect = requireControlType<Rect>(control, "Engine.Rect");
        rect.resize(size);
        rect.setScale(renderScale);
    };
    registerAdapter<RectUiControlAdapterTag>(std::move(rect));

    Adapter plainText;
    plainText.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<PlainText> result =
            std::make_shared<PlainText>(plainTextControlConfig(properties),
                                        stringProperty(properties, "text"));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    plainText.setter = [](ControlBase& control, const std::string& propertyId,
                          const RuntimeValue& value) {
        PlainText& text =
            requireControlType<PlainText>(control, "Engine.PlainText");
        if (propertyId == "text") {
            text.setString(requireString(value, "text"));
        } else if (propertyId == "colour") {
            text.setColour(requireColor(value, "colour"));
        } else {
            throw std::invalid_argument(
                propertyId + " is a construction-only PlainText property");
        }
    };
    plainText.arranger = [](ControlBase& control, const sf::Vector2f&,
                            const sf::Vector2f& renderScale) {
        arrangeAtIntrinsicSize(control, renderScale);
    };
    registerAdapter<PlainTextUiControlAdapterTag>(std::move(plainText));

    Adapter functionalPlainText;
    functionalPlainText.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<FunctionalPlainText> result =
            std::make_shared<FunctionalPlainText>(
                plainTextControlConfig(properties),
                stringProperty(properties, "text"));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    functionalPlainText.setter = [](ControlBase& control,
                                    const std::string& propertyId,
                                    const RuntimeValue& value) {
        FunctionalPlainText& text = requireControlType<FunctionalPlainText>(
            control, "Engine.FunctionalPlainText");
        if (propertyId == "text") {
            text.setString(requireString(value, "text"));
        } else if (propertyId == "colour") {
            text.setColour(requireColor(value, "colour"));
        } else {
            throw std::invalid_argument(
                propertyId +
                " is a construction-only FunctionalPlainText property");
        }
    };
    functionalPlainText.arranger = [](ControlBase& control, const sf::Vector2f&,
                                      const sf::Vector2f& renderScale) {
        arrangeAtIntrinsicSize(control, renderScale);
    };
    registerAdapter<FunctionalPlainTextUiControlAdapterTag>(
        std::move(functionalPlainText));

    Adapter richText;
    richText.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<RichText> result = std::make_shared<RichText>(
            richTextConfig(stringProperty(properties, "textConfig")),
            stringProperty(properties, "text"));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    richText.setter = [](ControlBase& control, const std::string& propertyId,
                         const RuntimeValue& value) {
        RichText& text =
            requireControlType<RichText>(control, "Engine.RichText");
        if (propertyId == "text") {
            text.setString(requireString(value, "text"));
        } else if (propertyId == "colour") {
            text.setColour(requireColor(value, "colour"));
        } else {
            throw std::invalid_argument(
                propertyId + " is a construction-only RichText property");
        }
    };
    richText.arranger = [](ControlBase& control, const sf::Vector2f&,
                           const sf::Vector2f& renderScale) {
        arrangeAtIntrinsicSize(control, renderScale);
    };
    registerAdapter<RichTextUiControlAdapterTag>(std::move(richText));

    Adapter functionalRichText;
    functionalRichText.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<FunctionalRichText> result =
            std::make_shared<FunctionalRichText>(
                richTextConfig(stringProperty(properties, "textConfig")),
                stringProperty(properties, "text"));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    functionalRichText.setter = [](ControlBase& control,
                                   const std::string& propertyId,
                                   const RuntimeValue& value) {
        FunctionalRichText& text = requireControlType<FunctionalRichText>(
            control, "Engine.FunctionalRichText");
        if (propertyId == "text") {
            text.setString(requireString(value, "text"));
        } else if (propertyId == "colour") {
            text.setColour(requireColor(value, "colour"));
        } else {
            throw std::invalid_argument(
                propertyId +
                " is a construction-only FunctionalRichText property");
        }
    };
    functionalRichText.arranger = [](ControlBase& control, const sf::Vector2f&,
                                     const sf::Vector2f& renderScale) {
        arrangeAtIntrinsicSize(control, renderScale);
    };
    registerAdapter<FunctionalRichTextUiControlAdapterTag>(
        std::move(functionalRichText));
}

const UiControlAdapterRegistry::Adapter&
UiControlAdapterRegistry::requireAdapter(const std::string& controlId) const {
    const auto iterator = adapters_.find(controlId);
    if (iterator == adapters_.end()) {
        throw std::invalid_argument("Unknown UI control adapter: " + controlId);
    }
    return iterator->second;
}
