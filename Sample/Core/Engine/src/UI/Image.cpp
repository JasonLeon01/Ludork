#include <UI/Image.hpp>

#include <Runtime/EngineState.hpp>

#include <utility>

Image::Image(std::shared_ptr<sf::Texture> texture,
             std::optional<sf::IntRect> rect)
    : SpriteBase(std::move(texture), rect) {}

void Image::_applyRenderStates(sf::RenderStates& states) const {
    SpriteBase::_applyRenderStates(states);
    states.transform.scale({Scale, Scale});
}

sf::Transform Image::_getRenderTransform() const {
    sf::Transform transform = SpriteBase::_getRenderTransform();
    transform.scale({Scale, Scale});
    return transform;
}
