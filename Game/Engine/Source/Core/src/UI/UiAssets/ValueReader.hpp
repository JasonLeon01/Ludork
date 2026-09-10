#pragma once

#include <CoreMinimal.hpp>
#include <Runtime/RuntimeData.hpp>

namespace ludork::engine::ui_asset_runtime_impl {

sf::Vector2f requireVector2f(const RuntimeData& value,
                             const std::string& source);

}  // namespace ludork::engine::ui_asset_runtime_impl
