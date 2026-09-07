#pragma once

#include <CoreMinimal.hpp>
#include <Runtime/RuntimeData.hpp>

namespace ludork::engine::ui_asset_runtime_impl {

using AssetLoader = std::function<RuntimeData(const std::string& assetKey)>;

struct BuildContext {
    const AssetLoader& loader;
    bool designMode = false;
    std::vector<std::string> assetStack;
};

}  // namespace ludork::engine::ui_asset_runtime_impl
