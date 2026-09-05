#pragma once

#include <EngineRuntimeApi.hpp>

#include <SFML/System/Vector2.hpp>

namespace ludork::engine::ui_asset_runtime_impl {
struct AssetState;
}

using UiAssetInstanceState = ludork::engine::ui_asset_runtime_impl::AssetState;

class LUDORK_ENGINE_API UiLayoutEngine {
public:
    static void reflow(UiAssetInstanceState& state,
                       const sf::Vector2f& logicalSize);
};
