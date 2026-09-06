#pragma once

#include <EngineRuntimeApi.hpp>

#include <SFML/System/Vector2.hpp>

namespace ludork::engine::ui_asset_runtime_impl {
struct AssetImpl;
}

using UiAssetInstanceState = ludork::engine::ui_asset_runtime_impl::AssetImpl;

class LUDORK_ENGINE_API UiLayoutEngine {
public:
    static void reflow(UiAssetInstanceState& impl,
                       const sf::Vector2f& logicalSize);
};
