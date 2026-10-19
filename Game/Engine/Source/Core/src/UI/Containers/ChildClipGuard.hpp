#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/Graphics/View.hpp>

class ControlBase;

namespace ludork::engine::ui_container {

class ChildClipGuard {
public:
    ChildClipGuard(sf::RenderTarget& target, const ControlBase& child,
                   const sf::Transform& parentTransform);
    ~ChildClipGuard();

    ChildClipGuard(const ChildClipGuard&) = delete;
    ChildClipGuard& operator=(const ChildClipGuard&) = delete;

    bool empty() const;

private:
    sf::RenderTarget& target_;
    sf::View view_;
    bool changed_ = false;
    bool empty_ = false;
};

}  // namespace ludork::engine::ui_container
