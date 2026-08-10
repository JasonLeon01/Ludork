#include <UI/Window.hpp>

#include <Runtime/EngineState.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <stdexcept>

Window::Window(const sf::IntRect& rect, const sf::Image& windowSkin,
               bool repeated)
    : SpriteBase(placeholderTexture()),
      size_(logicalSize(rect.size)),
      canvas_(std::make_shared<sf::RenderTexture>(
          nonZeroRenderTextureSize(scaledSize(rect.size)))),
      windowSkin_(windowSkin),
      repeated_(repeated),
      windowEdge_(canvas_->getSize()) {
    setPremultipliedTexture(true);
    initUi();
    bindCanvasTexture();
    setPosition({static_cast<float>(rect.position.x),
                 static_cast<float>(rect.position.y)});
}

sf::Vector2f Window::getSize() const {
    return {static_cast<float>(size_.x), static_cast<float>(size_.y)};
}

void Window::resize(const sf::Vector2u& size) {
    const sf::Vector2u logicalTextureSize{
        static_cast<unsigned int>(static_cast<float>(size.x) * Scale),
        static_cast<unsigned int>(static_cast<float>(size.y) * Scale),
    };
    const sf::Vector2u backingTextureSize =
        nonZeroRenderTextureSize(logicalTextureSize);
    if (size_ == size && canvas_->getSize() == backingTextureSize &&
        windowEdge_.getSize() == backingTextureSize) {
        return;
    }
    size_ = size;
    if (!canvas_->resize(backingTextureSize) ||
        !windowEdge_.resize(backingTextureSize)) {
        throw std::runtime_error("Failed to resize window render textures");
    }
    initUi();
    bindCanvasTexture();
}

void Window::setWindowSkin(const sf::Image& windowSkin, bool repeated) {
    windowSkin_ = windowSkin;
    repeated_ = repeated;
    initUi();
    bindCanvasTexture();
}

std::shared_ptr<sf::Texture> Window::placeholderTexture() {
    return std::make_shared<sf::Texture>();
}

sf::Vector2u Window::logicalSize(const sf::Vector2i& size) {
    return {static_cast<unsigned int>(std::max(0, size.x)),
            static_cast<unsigned int>(std::max(0, size.y))};
}

sf::Vector2u Window::scaledSize(const sf::Vector2i& size) {
    return {
        static_cast<unsigned int>(
            std::max(0.0f, static_cast<float>(size.x) * Scale)),
        static_cast<unsigned int>(
            std::max(0.0f, static_cast<float>(size.y) * Scale)),
    };
}

void Window::initUi() {
    windowEdgeSprite_ = std::make_unique<sf::Sprite>(windowEdge_.getTexture());
    const sf::Vector2u canvasSize = canvas_->getSize();
    windowBackTexture_ = std::make_unique<sf::Texture>(
        windowSkin_, false, sf::IntRect({0, 0}, {128, 128}));
    windowBackTexture_->setRepeated(repeated_);
    windowBackSprite_ = std::make_unique<sf::Sprite>(*windowBackTexture_);
    if (repeated_) {
        windowBackSprite_->setScale({Scale, Scale});
        windowBackSprite_->setTextureRect(
            sf::IntRect({0, 0}, {static_cast<int>(canvasSize.x / Scale),
                                 static_cast<int>(canvasSize.y / Scale)}));
    } else {
        windowBackSprite_->setScale(
            {static_cast<float>(canvasSize.x) / 128.0f,
             static_cast<float>(canvasSize.y) / 128.0f});
    }

    cacheTextures(cachedCorners_, {
                                      sf::IntRect({128, 0}, {16, 16}),
                                      sf::IntRect({176, 0}, {16, 16}),
                                      sf::IntRect({128, 48}, {16, 16}),
                                      sf::IntRect({176, 48}, {16, 16}),
                                  });
    cacheTextures(cachedEdges_, {
                                    sf::IntRect({144, 0}, {24, 16}),
                                    sf::IntRect({144, 48}, {24, 16}),
                                    sf::IntRect({128, 16}, {16, 24}),
                                    sf::IntRect({176, 16}, {16, 24}),
                                });
    for (sf::Texture& edge : cachedEdges_) {
        edge.setRepeated(true);
    }

    rectImpl_.render(*canvas_, windowEdge_, *windowEdgeSprite_,
                     *windowBackSprite_, texturePointers(cachedCorners_),
                     texturePointers(cachedEdges_), canvasRenderStates());
}

void Window::bindCanvasTexture() {
    std::shared_ptr<sf::Texture> texture(
        canvas_, const_cast<sf::Texture*>(&canvas_->getTexture()));
    setTexture(std::move(texture), true);
    const sf::Vector2u textureSize =
        scaledSize({static_cast<int>(size_.x), static_cast<int>(size_.y)});
    setTextureRect(
        {{0, 0},
         {static_cast<int>(textureSize.x), static_cast<int>(textureSize.y)}});
}

void Window::cacheTextures(std::vector<sf::Texture>& target,
                           const std::vector<sf::IntRect>& areas) {
    target.clear();
    target.reserve(areas.size());
    for (const sf::IntRect& area : areas) {
        target.emplace_back(windowSkin_, false, area);
    }
}

std::vector<sf::Texture*> Window::texturePointers(
    std::vector<sf::Texture>& textures) {
    std::vector<sf::Texture*> result;
    result.reserve(textures.size());
    for (sf::Texture& texture : textures) {
        result.push_back(&texture);
    }
    return result;
}
