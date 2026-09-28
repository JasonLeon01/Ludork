#pragma once

#include <CoreMinimal.hpp>
#include <unordered_set>
#include <Runtime/RuntimeData.hpp>

namespace ludork::engine::ui_asset_runtime_impl {

void requireOnlyKeys(const RuntimeData::Map& values,
                     const std::unordered_set<std::string>& allowed,
                     const std::string& source);

sf::Vector2f requireVector2f(const RuntimeData& value,
                             const std::string& source);

}  // namespace ludork::engine::ui_asset_runtime_impl
