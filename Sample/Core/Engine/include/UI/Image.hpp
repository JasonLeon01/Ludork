#pragma once

#include <BindAnnotations.hpp>
#include <UI/SpriteBase.hpp>

BIND_CLASS(callbacks = "_applyRenderStates,_getRenderTransform")
class Image : public SpriteBase {
public:
    BIND_INIT()
    explicit Image(std::shared_ptr<sf::Texture> texture,
                   std::optional<sf::IntRect> rect = std::nullopt);
    virtual ~Image() = default;

protected:
    BIND_METHOD(metadata = false)
    virtual void _applyRenderStates(sf::RenderStates& states) const override;

    BIND_METHOD(metadata = false)
    virtual sf::Transform _getRenderTransform() const override;
};
