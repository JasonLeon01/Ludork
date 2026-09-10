#include <UI/UiLayoutEngine.hpp>

#include "UiAssets/AssetImpl.hpp"
#include "UiLayoutEngineImpl.hpp"

#include <UI/UiControlAdapterRegistry.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace {

ui_layout_engine_detail::AxisArrangement arrangeAxis(
    float parentSize, float anchorMinimum, float anchorMaximum,
    float offsetStart, float offsetEnd, float alignment, float desiredSize,
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

void layoutInstance(UiAssetInstanceState& impl,
                    const sf::Vector2f& logicalSize) {
    impl.logicalSize = logicalSize;
    UiRuntimeNode& root = *impl.root;
    if (root.nestedImpl != nullptr) {
        UiLayoutEngine::reflow(*root.nestedImpl, logicalSize);
    } else {
        UiControlAdapterRegistry::instance().arrange(
            root.controlId, *root.control, logicalSize, root.renderScale);
        layoutNode(impl.root);
    }
    if (logicalSize.x <= 0.0f || logicalSize.y <= 0.0f) {
        root.control->setScale({0.0f, 0.0f});
    }
    impl.layoutDirty = false;
}

void layoutCanvas(const std::shared_ptr<UiRuntimeNode>& node) {
    const sf::Vector2f parentSize = node->control->getSize();
    const UiControlAdapterRegistry& registry =
        UiControlAdapterRegistry::instance();
    for (const std::shared_ptr<UiRuntimeNode>& child : node->children) {
        const sf::Vector2f desired = registry.measure(*child->control);
        const UiCanvasSlotData& slot = child->canvasSlot;
        const ui_layout_engine_detail::AxisArrangement horizontal =
            arrangeAxis(parentSize.x, slot.anchorMinimum.x,
                        slot.anchorMaximum.x, slot.offsetLeft, slot.offsetRight,
                        slot.alignment.x, desired.x, slot.autoSize);
        const ui_layout_engine_detail::AxisArrangement vertical =
            arrangeAxis(parentSize.y, slot.anchorMinimum.y,
                        slot.anchorMaximum.y, slot.offsetTop, slot.offsetBottom,
                        slot.alignment.y, desired.y, slot.autoSize);
        const sf::Vector2f arrangedSize{horizontal.size, vertical.size};
        if (child->nestedImpl != nullptr) {
            UiLayoutEngine::reflow(*child->nestedImpl, arrangedSize);
        } else {
            registry.arrange(child->controlId, *child->control, arrangedSize,
                             child->renderScale);
            layoutNode(child);
        }
        const sf::Vector2f boundsOffset =
            child->control->getLocalBounds().position;
        child->control->setPosition(
            sf::Vector2f(horizontal.position, vertical.position) -
            boundsOffset);
    }
}

void layoutList(const std::shared_ptr<UiRuntimeNode>& node) {
    for (const std::shared_ptr<UiRuntimeNode>& child : node->children) {
        const sf::Vector2f childSize = child->control->getSize();
        if (child->nestedImpl != nullptr) {
            UiLayoutEngine::reflow(*child->nestedImpl, childSize);
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

void UiLayoutEngine::reflow(UiAssetInstanceState& impl,
                            const sf::Vector2f& logicalSize) {
    if (logicalSize.x < 0.0f || logicalSize.y < 0.0f) {
        throw std::invalid_argument("UI asset logical size cannot be negative");
    }
    layoutInstance(impl, logicalSize);
}
