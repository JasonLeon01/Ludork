#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <SFML/System/Vector2.hpp>

#include <memory>
#include <string>

class UiAssetInstance;

namespace ludork::preview_host {

sf::Vector2u designSize(const RuntimeValue::Map& asset);
std::shared_ptr<UiAssetInstance> instantiateUiPreview(
    const std::string& assetKey, const RuntimeValue& asset,
    const RuntimeValue::Map& dependencies, const sf::Vector2u& design,
    float renderScale);

}  // namespace ludork::preview_host
