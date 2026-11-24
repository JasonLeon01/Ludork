#pragma once

#include "UiControlAdapters/TextureLoadScope.hpp"
#include <Runtime/RuntimeData.hpp>

#include <memory>
#include <string>
#include <vector>

namespace ludork::engine::ui_asset_runtime_impl {

struct BuildContext {
    struct Resources {
        RuntimeData::Map assets;
        ui_control_adapter_detail::TextureLoadScope::Cache textures;
        bool snapshot = false;

        const RuntimeData& loadAsset(const std::string& assetKey);
    };

    std::shared_ptr<Resources> resources = std::make_shared<Resources>();
    bool designMode = false;
    std::vector<std::string> assetStack;
};

}  // namespace ludork::engine::ui_asset_runtime_impl
