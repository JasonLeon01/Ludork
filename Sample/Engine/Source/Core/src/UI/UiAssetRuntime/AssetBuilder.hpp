#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <SFML/System/Vector2.hpp>

#include <string>

namespace ludork::engine::ui_asset_runtime_impl {

bool isProjectControl(const std::string& controlId);
std::string nestedAssetKey(const std::string& controlId);
sf::Vector2f parseDesignSize(RuntimeMapView asset, const std::string& source);
RuntimeValue::Map effectiveProperties(RuntimeMapView node,
                                      RuntimeMapView properties,
                                      const std::string& controlId,
                                      bool designMode,
                                      const std::string& source);

}  // namespace ludork::engine::ui_asset_runtime_impl
