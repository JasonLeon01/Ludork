#pragma once

#include <LuaError.hpp>
#include <UI/ControlBase.hpp>
#include <UI/FunctionalBase.hpp>
#include <algorithm>
#include <memory>
#include <vector>

namespace ludork::engine::ui_container {

inline void tickChildren(const ControlBase& parent,
                         std::vector<std::shared_ptr<ControlBase>> children,
                         std::string_view callbackName,
                         void (FunctionalBase::*tick)(float), float deltaTime) {
    for (const std::shared_ptr<ControlBase>& child : children) {
        if (child->getParent().get() != &parent || !child->getVisible()) {
            continue;
        }
        if (FunctionalBase* functional =
                ludork::Cast<FunctionalBase>(child.get())) {
            ludork::standard::dispatchLuaCallback(callbackName, [&] {
                (functional->*tick)(deltaTime);
            });
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
