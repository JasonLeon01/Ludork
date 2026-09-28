#include "UI/UiPreviewInstantiation.hpp"
#include "Rendering/PreviewResources.hpp"

#include <EngineState.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <UI/UiAssetInstance.hpp>
#include <UI/UiAssetRuntime.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace ludork::preview_host {
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
    return UiAssetRuntime::instance().instantiateSnapshot(
        assetKey, asset, dependencies, design, true);
}

}  // namespace ludork::preview_host
