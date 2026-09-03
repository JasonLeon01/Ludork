#pragma once

#include <UI/ControlBase.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <functional>
#include <optional>
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

struct AnimationScalarKey {
    float time = 0.0f;
    float value = 0.0f;
};

struct AnimationVectorKey {
    float time = 0.0f;
    sf::Vector2f value;
};

struct AnimationColourKey {
    float time = 0.0f;
    sf::Color value = sf::Color::White;
};

struct AnimationDefinition {
    std::string name;
    std::optional<std::string> target;
    float duration = 0.0f;
    sf::Vector2f pivot{0.5f, 0.5f};
    std::vector<AnimationVectorKey> translation;
    std::vector<AnimationScalarKey> rotation;
    std::vector<AnimationVectorKey> scale;
    std::vector<AnimationColourKey> colour;
};

struct ActiveAnimation {
    std::shared_ptr<const AnimationDefinition> definition;
    std::shared_ptr<ControlBase> target;
    float elapsed = 0.0f;
    bool playing = true;
    std::function<void()> onFinished;
};

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
    std::unordered_map<std::string, std::shared_ptr<AnimationDefinition>>
        animations;
    std::unordered_map<std::string, std::shared_ptr<ActiveAnimation>>
        activeAnimations;
    std::weak_ptr<AssetState> parentState;
    std::string parentNodeName;
    bool layoutDirty = true;
};

}  // namespace ludork::engine::ui_asset_runtime_impl

using UiAssetInstanceState = ludork::engine::ui_asset_runtime_impl::AssetState;
using UiCanvasSlotData = ludork::engine::ui_asset_runtime_impl::CanvasSlotData;
using UiRuntimeNode = ludork::engine::ui_asset_runtime_impl::RuntimeNode;
