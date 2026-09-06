#pragma once

#include <CoreMinimal.hpp>

namespace ludork::engine::ui_asset_runtime_impl {

using AssetLoader = std::function<RuntimeValue(const std::string& assetKey)>;

struct BuildContext {
    const AssetLoader& loader;
    bool designMode = false;
    std::vector<std::string> assetStack;
};

}  // namespace ludork::engine::ui_asset_runtime_impl
