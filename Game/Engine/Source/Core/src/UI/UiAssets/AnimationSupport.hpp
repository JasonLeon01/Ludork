#pragma once

#include "AssetImpl.hpp"

namespace ludork::engine::ui_asset_runtime_impl {

struct ResolvedAnimation {
    std::shared_ptr<AssetImpl> owner;
    std::shared_ptr<const AnimationDefinition> definition;
    std::shared_ptr<ControlBase> target;
    std::string activeKey;
};

struct PendingCallback {
    std::string activeKey;
    std::shared_ptr<ActiveAnimation> active;
    std::function<void()> callback;
};

}  // namespace ludork::engine::ui_asset_runtime_impl
