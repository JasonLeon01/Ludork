#include "ChildClipGuard.hpp"

#include <UI/ControlBase.hpp>
#include <UI/ListView.hpp>

#include <optional>

namespace ludork::engine::ui_container {

ChildClipGuard::ChildClipGuard(sf::RenderTarget& target,
                               const ControlBase& child,
                               const sf::Transform& parentTransform)
    : target_(target), view_(target.getView()) {
    std::shared_ptr<ControlBase> parent = child.getParent();
    const ControlBase* descendant = &child;
    while (parent != nullptr) {
        if (const ListView* list = ludork::Cast<ListView>(parent.get());
            list != nullptr && list->getChildScrollingEnabled()) {
            break;
        }
        descendant = parent.get();
        parent = parent->getParent();
    }
    if (parent == nullptr) {
        return;
    }
    const sf::Transform screenToTarget =
        parentTransform *
        child.getParent()->screenRenderTransform().getInverse();
    const sf::FloatRect targetBounds =
        view_.getInverseTransform().transformRect(
            {{-1.0f, -1.0f}, {2.0f, 2.0f}});
    sf::FloatRect bounds =
        screenToTarget.getInverse().transformRect(targetBounds);
    while (parent != nullptr) {
        if (const ListView* list = ludork::Cast<ListView>(parent.get());
            list != nullptr && list->getChildScrollingEnabled()) {
            bounds = list->clipChildBounds(*descendant, bounds);
        }
        descendant = parent.get();
        parent = parent->getParent();
    }
    if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) {
        empty_ = true;
        return;
    }
    const sf::FloatRect normalized =
        (view_.getTransform() * screenToTarget).transformRect(bounds);
    const sf::FloatRect viewport = view_.getViewport();
    const sf::FloatRect scissor{
        {viewport.position.x +
             (normalized.position.x + 1.0f) * viewport.size.x / 2.0f,
         viewport.position.y +
             (1.0f - normalized.position.y - normalized.size.y) *
                 viewport.size.y / 2.0f},
        {normalized.size.x * viewport.size.x / 2.0f,
         normalized.size.y * viewport.size.y / 2.0f}};
    const std::optional<sf::FloatRect> intersection =
        scissor.findIntersection(view_.getScissor());
    if (!intersection.has_value()) {
        empty_ = true;
        return;
    }
    sf::View clippedView = view_;
    clippedView.setScissor(*intersection);
    target_.setView(clippedView);
    changed_ = true;
}

ChildClipGuard::~ChildClipGuard() {
    if (changed_) {
        target_.setView(view_);
    }
}

bool ChildClipGuard::empty() const {
    return empty_;
}

}  // namespace ludork::engine::ui_container
