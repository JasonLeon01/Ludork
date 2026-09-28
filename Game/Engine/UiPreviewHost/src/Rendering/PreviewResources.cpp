#include "PreviewResources.hpp"
#include "PreviewFontResource.hpp"

#include <Runtime/AssetStore.hpp>
#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <UI/UIState.hpp>

#include <filesystem>
#include <limits>
#include <stdexcept>
namespace ludork::preview_host {
namespace {

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

}  // namespace

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
    std::shared_ptr<PreviewFontResource> owner =
        std::make_shared<PreviewFontResource>();
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

sf::Vector2f previewGameSize() {
    const RuntimeData configValue = getJSONData(
        std::filesystem::path(".") / "Data" / "Configs" / "System.json");
    const RuntimeData::Map& config =
        ludork::runtime::value_reader::requireMap(configValue, "System config");
    const RuntimeData::Map& setting = ludork::runtime::value_reader::requireMap(
        ludork::runtime::value_reader::requireValue(config, "gameSize",
                                                    "System config"),
        "System config.gameSize");
    const RuntimeData::Array& size =
        ludork::runtime::value_reader::requireArray(
            ludork::runtime::value_reader::requireValue(
                setting, "value", "System config.gameSize"),
            "System config.gameSize.value");
    if (size.size() != 2) {
        throw std::invalid_argument(
            "System config.gameSize requires two dimensions");
    }
    const float width =
        ludork::runtime::value_reader::requireFloat(size[0], "gameSize.width");
    const float height =
        ludork::runtime::value_reader::requireFloat(size[1], "gameSize.height");
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("System config.gameSize must be positive");
    }
    return {width, height};
}

}  // namespace ludork::preview_host
