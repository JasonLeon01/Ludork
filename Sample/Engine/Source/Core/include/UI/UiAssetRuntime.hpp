#pragma once

#include <CoreMinimal.hpp>
#include <Runtime/RuntimeData.hpp>
#include <EngineRuntimeApi.hpp>

class UiAssetInstance;

class LUDORK_ENGINE_API UiAssetRuntime {
public:
    static UiAssetRuntime& instance();

    std::shared_ptr<UiAssetInstance> instantiate(
        const std::string& assetKey,
        std::optional<sf::Vector2u> logicalSize = std::nullopt) const;

    std::shared_ptr<UiAssetInstance> instantiateSnapshot(
        const std::string& assetKey, const RuntimeData& asset,
        const RuntimeData::Map& dependencies,
        std::optional<sf::Vector2u> logicalSize = std::nullopt,
        bool designMode = true) const;
};

BIND_FUNCTION(name = "instantiate", defaults = {nil})
LUDORK_ENGINE_API std::shared_ptr<UiAssetInstance> instantiateUiAsset(
    const std::string& assetKey,
    std::optional<sf::Vector2u> logicalSize = std::nullopt);
