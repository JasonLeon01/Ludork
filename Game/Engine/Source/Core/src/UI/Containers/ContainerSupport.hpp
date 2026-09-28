#pragma once

#include <UI/ControlBase.hpp>
#include <UI/FunctionalBase.hpp>
#include <algorithm>
#include <memory>
#include <vector>

namespace ludork::engine::ui_container {

template <typename Tick>
void tickChildren(const std::vector<std::shared_ptr<ControlBase>>& children,
                  Tick tick, float deltaTime) {
    for (const std::shared_ptr<ControlBase>& child : children) {
        if (child->getVisible()) {
            if (FunctionalBase* functional =
                    ludork::Cast<FunctionalBase>(child.get())) {
                (functional->*tick)(deltaTime);
            }
        }
    }
}

inline sf::FloatRect aggregateContentBounds(
    sf::FloatRect bounds,
    const std::vector<std::shared_ptr<ControlBase>>& children) {
    for (const std::shared_ptr<ControlBase>& child : children) {
        if (child == nullptr || !child->getVisible()) {
            continue;
        }
        const sf::FloatRect childBounds =
            child->getTransform().transformRect(child->getContentBounds());
        const float minimumX =
            std::min(bounds.position.x, childBounds.position.x);
        const float minimumY =
            std::min(bounds.position.y, childBounds.position.y);
        const float maximumX =
            std::max(bounds.position.x + bounds.size.x,
                     childBounds.position.x + childBounds.size.x);
        const float maximumY =
            std::max(bounds.position.y + bounds.size.y,
                     childBounds.position.y + childBounds.size.y);
        bounds = {{minimumX, minimumY},
                  {maximumX - minimumX, maximumY - minimumY}};
    }
    return bounds;
}

}  // namespace ludork::engine::ui_container
