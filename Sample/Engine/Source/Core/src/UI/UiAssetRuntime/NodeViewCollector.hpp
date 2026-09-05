#pragma once

#include "RuntimeModel.hpp"

#include <SFML/Graphics/Rect.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace ludork::engine::ui_asset_runtime_impl {

struct NodeView {
    std::string nodeName;
    std::shared_ptr<ControlBase> control;
    sf::FloatRect bounds;
    bool nestedBoundary = false;
    int zOrder = 0;
    std::size_t drawOrder = 0;
};

std::vector<NodeView> collectNodeViews(
    const std::shared_ptr<RuntimeNode>& root);

}  // namespace ludork::engine::ui_asset_runtime_impl
