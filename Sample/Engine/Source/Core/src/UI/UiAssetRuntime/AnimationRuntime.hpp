#pragma once

#include "RuntimeModel.hpp"

#include <Runtime/RuntimeValue.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace ludork::engine::ui_asset_runtime_impl {

void parseAnimations(RuntimeMapView asset, AssetState& state,
                     const std::string& source);

void installAnimationUpdater(const std::shared_ptr<AssetState>& state);

bool hasAnimation(const std::shared_ptr<AssetState>& state,
                  const std::string& name,
                  const std::optional<std::string>& target);

bool playAnimation(const std::shared_ptr<AssetState>& state,
                   const std::string& name,
                   const std::optional<std::string>& target,
                   std::function<void()> onFinished);

void stopAnimation(const std::shared_ptr<AssetState>& state,
                   const std::string& name,
                   const std::optional<std::string>& target);

bool sampleAnimation(const std::shared_ptr<AssetState>& state,
                     const std::string& name,
                     const std::optional<std::string>& target, float time);

void stopAllAnimations(const std::shared_ptr<AssetState>& state);

}  // namespace ludork::engine::ui_asset_runtime_impl
