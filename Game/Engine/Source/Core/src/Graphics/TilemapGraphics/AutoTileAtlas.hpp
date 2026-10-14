#pragma once

#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Rect.hpp>

#include <array>
#include <memory>
#include <vector>

namespace ludork::engine::tilemap_graphics_impl {

class AutoTileAtlas {
public:
    struct Frame {
        std::shared_ptr<sf::Texture> texture;
        sf::IntRect rectangle;
    };

    AutoTileAtlas(const sf::Texture& source, int cellSize, int frameCount);
    const Frame& frame(int mask, int animationFrame) const;

private:
    std::array<int, 256> patterns_{};
    std::vector<Frame> frames_;
    int patternCount_ = 0;
    int animationFrames_ = 1;
};

}  // namespace ludork::engine::tilemap_graphics_impl
