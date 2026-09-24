#pragma once

#include <Gameplay/Components/BillboardComponent.hpp>
#include <Curve.hpp>

struct BillboardComponent::Impl {
    struct ItemVisual {
        std::shared_ptr<sf::Texture> texture;
        std::unique_ptr<sf::Sprite> sprite;
        std::unique_ptr<sf::Text> text;
        sf::Color color = sf::Color::White;
        sf::FloatRect bounds;
    };

    void refresh(const std::vector<BillboardItem>& items);
    void loadCurves();
    void reset() noexcept;
    void advance(float deltaTime, bool inRange);
    void draw(sf::RenderTarget& target, sf::RenderStates states,
              sf::Vector2f head);

    std::vector<BillboardItem> cachedItems;
    std::vector<ItemVisual> visuals;
    std::shared_ptr<sf::Font> font;
    std::array<std::shared_ptr<Curve>, 4> curves;
    float height = 0.0f;
    float elapsed = 0.0f;
    float y = 16.0f;
    float alpha = 0.0f;
    float initialY = 16.0f;
    float initialAlpha = 0.0f;
    bool targetVisible = false;
    bool transitioning = false;
};
