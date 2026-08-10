#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Graphics/RectBase.hpp>
#include <UI/SpriteBase.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <memory>
#include <vector>

BIND_CLASS(callbacks = "getSize")
class LUDORK_ENGINE_API Window : public SpriteBase {
public:
    BIND_INIT()
    Window(const sf::IntRect& rect, const sf::Image& windowSkin,
           bool repeated = false);
    virtual ~Window() = default;

    BIND_METHOD(Pure = true, lua_return_type = "sf::Vector2u")
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD()
    void resize(const sf::Vector2u& size);

    BIND_METHOD()
    void setWindowSkin(const sf::Image& windowSkin, bool repeated = false);

private:
    static std::shared_ptr<sf::Texture> placeholderTexture();
    static sf::Vector2u logicalSize(const sf::Vector2i& size);
    static sf::Vector2u scaledSize(const sf::Vector2i& size);

    void initUi();
    void bindCanvasTexture();
    void cacheTextures(std::vector<sf::Texture>& target,
                       const std::vector<sf::IntRect>& areas);
    static std::vector<sf::Texture*> texturePointers(
        std::vector<sf::Texture>& textures);

    sf::Vector2u size_;
    std::shared_ptr<sf::RenderTexture> canvas_;
    sf::Image windowSkin_;
    bool repeated_ = false;
    RectBase rectImpl_;
    sf::RenderTexture windowEdge_;
    std::unique_ptr<sf::Sprite> windowEdgeSprite_;
    std::unique_ptr<sf::Texture> windowBackTexture_;
    std::unique_ptr<sf::Sprite> windowBackSprite_;
    std::vector<sf::Texture> cachedCorners_;
    std::vector<sf::Texture> cachedEdges_;
};
