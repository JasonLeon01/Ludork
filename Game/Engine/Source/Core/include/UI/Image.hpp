#pragma once

#include <CoreMinimal.hpp>

#include <UI/SpriteBase.hpp>

BIND_CLASS(callbacks = true)
class Image : public SpriteBase {
public:
    LUDORK_CAST_DERIVED(Image, SpriteBase)

    BIND_ENUM(name = "ImageDrawAs")
    enum class DrawAs {
        Image,
        Tile,
    };

    BIND_INIT()
    explicit Image(std::shared_ptr<sf::Texture> texture,
                   std::optional<sf::IntRect> rect = std::nullopt);
    virtual ~Image() = default;

    BIND_METHOD(Pure = true)
    BIND_METHOD(property = "drawAs", setter = "setDrawAs")
    DrawAs getDrawAs() const;

    BIND_METHOD()
    void setDrawAs(DrawAs drawAs);

protected:
    void onTextureChanged() override;

    BIND_METHOD()
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    BIND_METHOD(metadata = false)
    virtual void _applyRenderStates(sf::RenderStates& states) const override;

    BIND_METHOD(metadata = false)
    virtual sf::Transform _getRenderTransform() const override;

private:
    void prepareTileTexture() const;

    DrawAs drawAs_ = DrawAs::Image;
    mutable std::unique_ptr<sf::Texture> tileTexture_;
};
