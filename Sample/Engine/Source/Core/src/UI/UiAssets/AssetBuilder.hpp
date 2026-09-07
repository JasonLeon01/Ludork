#pragma once

#include <Runtime/RuntimeData.hpp>

#include <SFML/System/Vector2.hpp>

#include <string>

namespace ludork::engine::ui_asset_runtime_impl {

bool isProjectControl(const std::string& controlId);
std::string nestedAssetKey(const std::string& controlId);
sf::Vector2f parseDesignSize(const RuntimeData::Map& asset,
                             const std::string& source);
RuntimeData::Map effectiveProperties(const RuntimeData::Map& node,
                                     const RuntimeData::Map& properties,
                                     const std::string& controlId,
                                     bool designMode,
                                     const std::string& source);

}  // namespace ludork::engine::ui_asset_runtime_impl
