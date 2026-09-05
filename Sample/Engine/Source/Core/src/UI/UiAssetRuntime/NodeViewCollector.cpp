#include "NodeViewCollector.hpp"

#include <UI/UiControlAdapterRegistry.hpp>

#include <algorithm>

namespace ludork::engine::ui_asset_runtime_impl {
namespace {

void collect(const std::shared_ptr<RuntimeNode>& node,
             std::vector<NodeView>& result, std::size_t& drawOrder) {
    NodeView view;
    view.nodeName = node->name;
    view.control = node->control;
    view.bounds = node->control->getAbsoluteBounds();
    view.nestedBoundary = node->nestedState != nullptr;
    view.zOrder = node->canvasSlot.zOrder;
    view.drawOrder = drawOrder++;
    result.push_back(std::move(view));
    if (node->nestedState != nullptr) {
        return;
    }
    std::vector<std::shared_ptr<RuntimeNode>> children = node->children;
    if (UiControlAdapterRegistry::instance().slotType(node->controlId) ==
        UiControlSlotType::Canvas) {
        std::stable_sort(children.begin(), children.end(),
                         [](const std::shared_ptr<RuntimeNode>& left,
                            const std::shared_ptr<RuntimeNode>& right) {
                             return left->canvasSlot.zOrder <
                                    right->canvasSlot.zOrder;
                         });
    }
    for (const std::shared_ptr<RuntimeNode>& child : children) {
        collect(child, result, drawOrder);
    }
}

}  // namespace

std::vector<NodeView> collectNodeViews(
    const std::shared_ptr<RuntimeNode>& root) {
    std::vector<NodeView> result;
    std::size_t drawOrder = 0;
    collect(root, result, drawOrder);
    return result;
}

}  // namespace ludork::engine::ui_asset_runtime_impl
