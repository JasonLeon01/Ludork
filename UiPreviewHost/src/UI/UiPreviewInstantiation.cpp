#include "UI/UiPreviewInstantiation.hpp"

#include "Protocol/PreviewProtocol.hpp"

#include <EngineState.hpp>
#include <Runtime/RuntimeValueReader.hpp>
#include <UI/UiAssetRuntime.hpp>
#include <UI/UIState.hpp>
#include <Utf8Path.hpp>
#include <Utils/File.hpp>

#include <SFML/Graphics/Font.hpp>

#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace ludork::preview_host {
namespace {

std::filesystem::path safeProjectPath(const std::string& value,
                                      const std::string& source) {
    const std::filesystem::path relative =
        ludork::standard::pathFromUtf8(value).lexically_normal();
    if (relative.empty() || relative.is_absolute()) {
        throw std::invalid_argument(source +
                                    " must be a relative project path");
    }
    for (const std::filesystem::path& part : relative) {
        if (part == "..") {
            throw std::invalid_argument(source +
                                        " cannot traverse parent folders");
        }
    }
    return std::filesystem::current_path() / relative;
}

std::string settingString(const RuntimeValue::Map& config,
                          const std::string& name) {
    const RuntimeValue::Map& setting =
        ludork::runtime::value_reader::requireMap(
            ludork::runtime::value_reader::requireValue(config, name,
                                                        "System config"),
            "System config." + name);
    return ludork::runtime::value_reader::requireString(
        ludork::runtime::value_reader::requireValue(setting, "value",
                                                    "System config." + name),
        "System config." + name + ".value");
}

std::int64_t settingInteger(const RuntimeValue::Map& config,
                            const std::string& name) {
    const RuntimeValue::Map& setting =
        ludork::runtime::value_reader::requireMap(
            ludork::runtime::value_reader::requireValue(config, name,
                                                        "System config"),
            "System config." + name);
    return ludork::runtime::value_reader::requireInteger(
        ludork::runtime::value_reader::requireValue(setting, "value",
                                                    "System config." + name),
        "System config." + name + ".value");
}

void configureUiResources() {
    const RuntimeValue configValue = getJSONData(
        std::filesystem::path(".") / "Data" / "Configs" / "System.json");
    const RuntimeValue::Map& config = ludork::runtime::value_reader::requireMap(
        configValue, "Data/Configs/System.json");
    const RuntimeValue::Map& fonts = ludork::runtime::value_reader::requireMap(
        ludork::runtime::value_reader::requireValue(config, "fonts",
                                                    "System config"),
        "System config.fonts");
    const RuntimeValue::Array& fontNames =
        ludork::runtime::value_reader::requireArray(
            ludork::runtime::value_reader::requireValue(fonts, "value",
                                                        "System config.fonts"),
            "System config.fonts.value");
    if (fontNames.empty()) {
        throw std::invalid_argument(
            "System config must declare at least one font");
    }
    const std::string& fontName = ludork::runtime::value_reader::requireString(
        fontNames.front(), "System config.fonts.value[0]");
    const RuntimeValue* baseValue =
        ludork::runtime::value_reader::findValue(fonts, "base");
    const std::string base = baseValue == nullptr
                                 ? "Fonts"
                                 : ludork::runtime::value_reader::requireString(
                                       *baseValue, "System config.fonts.base");
    const std::filesystem::path fontPath = safeProjectPath(
        "Assets/" + base + "/" + fontName, "System config font");
    std::shared_ptr<sf::Font> font = std::make_shared<sf::Font>();
    if (!font->openFromFile(fontPath)) {
        throw std::runtime_error("Failed to load preview font: " +
                                 ludork::standard::pathToUtf8(fontPath));
    }
    const std::int64_t size = settingInteger(config, "fontSize");
    if (size <= 0 || size > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(
            "System config fontSize must be a positive integer");
    }
    defaultFont = std::move(font);
    defaultFontSize = static_cast<int>(size);
    defaultWindowskinName = settingString(config, "windowskinName");
}

}  // namespace

sf::Vector2u designSize(const RuntimeValue::Map& asset) {
    const RuntimeValue::Map& size = ludork::runtime::value_reader::requireMap(
        ludork::runtime::value_reader::requireValue(asset, "designSize",
                                                    "UI asset"),
        "UI asset.designSize");
    const double width = ludork::runtime::value_reader::requireNumber(
        ludork::runtime::value_reader::requireValue(size, "width",
                                                    "UI asset.designSize"),
        "UI asset.designSize.width");
    const double height = ludork::runtime::value_reader::requireNumber(
        ludork::runtime::value_reader::requireValue(size, "height",
                                                    "UI asset.designSize"),
        "UI asset.designSize.height");
    if (width <= 0.0 || height <= 0.0 ||
        width > static_cast<double>(std::numeric_limits<unsigned int>::max()) ||
        height >
            static_cast<double>(std::numeric_limits<unsigned int>::max())) {
        throw std::invalid_argument(
            "UI asset designSize is outside the preview range");
    }
    const unsigned int roundedWidth =
        static_cast<unsigned int>(std::lround(width));
    const unsigned int roundedHeight =
        static_cast<unsigned int>(std::lround(height));
    if (roundedWidth == 0 || roundedHeight == 0) {
        throw std::invalid_argument(
            "UI asset designSize must produce positive pixels");
    }
    return {roundedWidth, roundedHeight};
}

std::shared_ptr<UiAssetInstance> instantiateUiPreview(
    const std::string& assetKey, const RuntimeValue& asset,
    const RuntimeValue::Map& dependencies, const sf::Vector2u& design,
    float renderScale) {
    engineState().setScale(renderScale);
    configureUiResources();
    return UiAssetRuntime::instance().instantiateSnapshot(
        assetKey, asset, dependencies, design, true);
}

}  // namespace ludork::preview_host
