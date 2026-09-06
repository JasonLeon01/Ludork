#pragma once

#include <filesystem>
#include <string>

namespace ludork::engine::ui_asset_runtime_impl {

std::filesystem::path validateLogicalAssetKey(const std::string& assetKey);
std::filesystem::path assetPath(const std::string& assetKey);

}  // namespace ludork::engine::ui_asset_runtime_impl
