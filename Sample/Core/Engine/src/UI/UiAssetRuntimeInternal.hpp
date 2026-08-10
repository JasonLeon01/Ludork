#pragma once

#include <UI/UiAssetRuntime.hpp>

#include <SFML/System/Vector2.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct UiCanvasSlotData {
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

struct UiRuntimeNode {
    std::string id;
    std::string name;
    std::string controlId;
    std::shared_ptr<ControlBase> control;
    std::shared_ptr<UiAssetInstance> nestedAsset;
    std::vector<std::shared_ptr<UiRuntimeNode>> children;
    UiCanvasSlotData canvasSlot;
    sf::Vector2f renderScale = {1.0f, 1.0f};
};

struct UiAssetInstanceState {
    std::string assetKey;
    sf::Vector2f designSize;
    sf::Vector2f logicalSize;
    std::shared_ptr<UiRuntimeNode> root;
    std::unordered_map<std::string, std::shared_ptr<UiRuntimeNode>> controls;
    std::unordered_map<std::string, std::shared_ptr<UiAssetInstance>>
        nestedAssets;
    bool layoutDirty = true;
};
