#include "UI/UiPreviewInstantiation.hpp"

#include "Protocol/PreviewProtocol.hpp"

#include <EngineState.hpp>
#include <Runtime/AssetStore.hpp>
#include <Runtime/RuntimeValueReader.hpp>
#include <UI/UiAssetRuntime.hpp>
#include <UI/UIState.hpp>
#include <Runtime/Json.hpp>

#include <SFML/Graphics/Font.hpp>

#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace ludork::preview_host {
namespace {

struct FontResource {
    std::unique_ptr<ludork::runtime::AssetInputStream> stream;
    sf::Font font;
};

std::string settingString(const RuntimeData::Map& config,
                          const std::string& name) {
    const RuntimeData::Map& setting = ludork::runtime::value_reader::requireMap(
        ludork::runtime::value_reader::requireValue(config, name,
                                                    "System config"),
        "System config." + name);
    return ludork::runtime::value_reader::requireString(
        ludork::runtime::value_reader::requireValue(setting, "value",
                                                    "System config." + name),
        "System config." + name + ".value");
}

std::int64_t settingInteger(const RuntimeData::Map& config,
                            const std::string& name) {
    const RuntimeData::Map& setting = ludork::runtime::value_reader::requireMap(
        ludork::runtime::value_reader::requireValue(config, name,
                                                    "System config"),
        "System config." + name);
    return ludork::runtime::value_reader::requireInteger(
        ludork::runtime::value_reader::requireValue(setting, "value",
                                                    "System config." + name),
        "System config." + name + ".value");
}

void configureUiResources() {
    const RuntimeData configValue = getJSONData(
        std::filesystem::path(".") / "Data" / "Configs" / "System.json");
    const RuntimeData::Map& config = ludork::runtime::value_reader::requireMap(
        configValue, "Data/Configs/System.json");
    const RuntimeData::Map& fonts = ludork::runtime::value_reader::requireMap(
        ludork::runtime::value_reader::requireValue(config, "fonts",
                                                    "System config"),
        "System config.fonts");
    const RuntimeData::Array& fontNames =
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
    std::shared_ptr<FontResource> owner = std::make_shared<FontResource>();
    owner->stream = ludork::runtime::assetStore().open(fontName);
    if (!owner->font.openFromStream(*owner->stream)) {
        throw std::runtime_error("Failed to load preview font: " + fontName);
    }
    const std::int64_t size = settingInteger(config, "fontSize");
    if (size <= 0 || size > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(
            "System config fontSize must be a positive integer");
    }
    defaultFont = std::shared_ptr<sf::Font>(owner, &owner->font);
    defaultFontSize = static_cast<int>(size);
    defaultWindowskinName = settingString(config, "windowskinName");
}

}  // namespace

sf::Vector2u designSize(const RuntimeData::Map& asset) {
    const RuntimeData::Map& size = ludork::runtime::value_reader::requireMap(
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
    const std::string& assetKey, const RuntimeData& asset,
    const RuntimeData::Map& dependencies, const sf::Vector2u& design,
    float renderScale) {
    engineState().setScale(renderScale);
    configureUiResources();
    const RuntimeValue dependencyValues{RuntimeData(dependencies)};
    return UiAssetRuntime::instance().instantiateSnapshot(
        assetKey, RuntimeValue(asset), dependencyValues.view().map()->toMap(),
        design, true);
}

}  // namespace ludork::preview_host
