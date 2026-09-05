#pragma once

#include <Runtime/RuntimeData.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class UiAssetInstance;

namespace ludork::preview_host {

struct RenderTargetSpec {
    float renderScale;
    sf::Vector2u size;
};

RenderTargetSpec renderTargetSpec(const sf::Vector2u& design,
                                  double requestedScale);
std::vector<std::uint8_t> renderFrame(
    const std::shared_ptr<UiAssetInstance>& instance, const sf::Vector2u& size);
RuntimeData::Array nodeGeometry(
    const std::shared_ptr<UiAssetInstance>& instance, const sf::Vector2u& size,
    float renderScale);
std::optional<std::string> hitTestUiPreview(
    const std::shared_ptr<UiAssetInstance>& instance,
    const sf::Vector2u& renderSize, float renderScale,
    const sf::Vector2f& logicalPoint);

}  // namespace ludork::preview_host
