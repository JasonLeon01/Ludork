#pragma once

#include <UI/ControlBase.hpp>

#include <SFML/System/Vector2.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ludork::engine::ui_asset_runtime_impl {

struct CanvasSlotData {
    sf::Vector2f anchorMinimum = {0.0f, 0.0f};
    sf::Vector2f anchorMaximum = {0.0f, 0.0f};
    float offsetLeft = 0.0f;
    float offsetTop = 0.0f;
    float offsetRight = 0.0f;
    float offsetBottom = 0.0f;
    sf::Vector2f alignment = {0.0f, 0.0f};
    bool autoSize = false;
    int zOrder = 0;
};

struct AssetState;

struct RuntimeNode {
    std::string name;
    std::string controlId;
    std::shared_ptr<ControlBase> control;
    std::shared_ptr<AssetState> nestedState;
    std::vector<std::shared_ptr<RuntimeNode>> children;
    CanvasSlotData canvasSlot;
    sf::Vector2f renderScale = {1.0f, 1.0f};
};

struct AssetState {
    std::string assetKey;
    sf::Vector2f designSize;
    sf::Vector2f logicalSize;
    std::shared_ptr<RuntimeNode> root;
    std::unordered_map<std::string, std::shared_ptr<RuntimeNode>> controls;
    std::unordered_map<std::string, std::shared_ptr<AssetState>> nestedStates;
    bool layoutDirty = true;
};

}  // namespace ludork::engine::ui_asset_runtime_impl

using UiAssetInstanceState = ludork::engine::ui_asset_runtime_impl::AssetState;
using UiCanvasSlotData = ludork::engine::ui_asset_runtime_impl::CanvasSlotData;
using UiRuntimeNode = ludork::engine::ui_asset_runtime_impl::RuntimeNode;
