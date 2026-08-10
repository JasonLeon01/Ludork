#include <UI/UiLayoutEngine.hpp>

#include "UiAssetRuntimeInternal.hpp"

#include <UI/UiControlAdapterRegistry.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace {

struct AxisArrangement {
    float position = 0.0f;
    float size = 0.0f;
};

AxisArrangement arrangeAxis(float parentSize, float anchorMinimum,
                            float anchorMaximum, float offsetStart,
                            float offsetEnd, float alignment, float desiredSize,
                            bool autoSize) {
    if (autoSize) {
        return {
            anchorMinimum * parentSize + offsetStart - alignment * desiredSize,
            desiredSize};
    }
    if (std::abs(anchorMaximum - anchorMinimum) <= 0.00001f) {
        const float size = std::max(0.0f, offsetEnd);
        return {anchorMinimum * parentSize + offsetStart - alignment * size,
                size};
    }
    const float start = anchorMinimum * parentSize + offsetStart;
    const float end = anchorMaximum * parentSize - offsetEnd;
    return {start, std::max(0.0f, end - start)};
}

void layoutNode(const std::shared_ptr<UiRuntimeNode>& node);

void layoutInstance(UiAssetInstanceState& state,
                    const sf::Vector2f& logicalSize) {
    state.logicalSize = logicalSize;
    UiRuntimeNode& root = *state.root;
    if (root.nestedAsset != nullptr) {
        UiLayoutEngine::reflow(*root.nestedAsset, logicalSize);
    } else {
        UiControlAdapterRegistry::instance().arrange(
            root.controlId, *root.control, logicalSize, root.renderScale);
        layoutNode(state.root);
    }
    if (logicalSize.x <= 0.0f || logicalSize.y <= 0.0f) {
        root.control->setScale({0.0f, 0.0f});
    }
    state.layoutDirty = false;
}

void layoutCanvas(const std::shared_ptr<UiRuntimeNode>& node) {
    const sf::Vector2f parentSize = node->control->getSize();
    const UiControlAdapterRegistry& registry =
        UiControlAdapterRegistry::instance();
    for (const std::shared_ptr<UiRuntimeNode>& child : node->children) {
        const sf::Vector2f desired = registry.measure(*child->control);
        const UiCanvasSlotData& slot = child->canvasSlot;
        const AxisArrangement horizontal =
            arrangeAxis(parentSize.x, slot.anchorMinimum.x,
                        slot.anchorMaximum.x, slot.offsetLeft, slot.offsetRight,
                        slot.alignment.x, desired.x, slot.autoSize);
        const AxisArrangement vertical =
            arrangeAxis(parentSize.y, slot.anchorMinimum.y,
                        slot.anchorMaximum.y, slot.offsetTop, slot.offsetBottom,
                        slot.alignment.y, desired.y, slot.autoSize);
        const sf::Vector2f arrangedSize{horizontal.size, vertical.size};
        if (child->nestedAsset != nullptr) {
            UiLayoutEngine::reflow(*child->nestedAsset, arrangedSize);
        } else {
            registry.arrange(child->controlId, *child->control, arrangedSize,
                             child->renderScale);
            layoutNode(child);
        }
        child->control->setPosition({horizontal.position, vertical.position});
    }
}

void layoutList(const std::shared_ptr<UiRuntimeNode>& node) {
    for (const std::shared_ptr<UiRuntimeNode>& child : node->children) {
        const sf::Vector2f childSize = child->control->getSize();
        if (child->nestedAsset != nullptr) {
            UiLayoutEngine::reflow(*child->nestedAsset, childSize);
        } else {
            layoutNode(child);
        }
    }
    UiControlAdapterRegistry::instance().reflowChildren(node->controlId,
                                                        *node->control);
}

void layoutNode(const std::shared_ptr<UiRuntimeNode>& node) {
    switch (UiControlAdapterRegistry::instance().slotType(node->controlId)) {
        case UiControlSlotType::Canvas:
            layoutCanvas(node);
            break;
        case UiControlSlotType::List:
            layoutList(node);
            break;
        case UiControlSlotType::None:
            break;
    }
}

}  // namespace

void UiLayoutEngine::reflow(UiAssetInstance& instance,
                            const sf::Vector2f& logicalSize) {
    if (logicalSize.x < 0.0f || logicalSize.y < 0.0f) {
        throw std::invalid_argument("UI asset logical size cannot be negative");
    }
    layoutInstance(*instance.state_, logicalSize);
}
