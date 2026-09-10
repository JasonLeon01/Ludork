#pragma once

#include <Runtime/RuntimeData.hpp>

#include <SFML/System/Vector2.hpp>

#include <memory>
#include <string>

class UiAssetInstance;

namespace ludork::preview_host {

sf::Vector2u designSize(const RuntimeData::Map& asset);
std::shared_ptr<UiAssetInstance> instantiateUiPreview(
    const std::string& assetKey, const RuntimeData& asset,
    const RuntimeData::Map& dependencies, const sf::Vector2u& design,
    float renderScale);

}  // namespace ludork::preview_host
