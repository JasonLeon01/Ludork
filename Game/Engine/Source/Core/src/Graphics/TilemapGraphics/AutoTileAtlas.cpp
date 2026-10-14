#include "AutoTileAtlas.hpp"
#include "Pattern.hpp"

#include <SFML/Graphics/Image.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ludork::engine::tilemap_graphics_impl {

AutoTileAtlas::AutoTileAtlas(const sf::Texture& source, int cellSize,
                             int frameCount)
    : animationFrames_(std::max(1, frameCount)) {
    if (cellSize <= 0 || cellSize % 2 != 0) {
        throw std::invalid_argument(
            "Autotile atlas requires a positive even cell size");
    }
    const sf::Image original = source.copyToImage();
    if (original.getSize().x <
            static_cast<unsigned>(3 * cellSize * animationFrames_) ||
        original.getSize().y < static_cast<unsigned>(4 * cellSize)) {
        throw std::invalid_argument(
            "Autotile source does not contain its declared animation frames");
    }
    std::vector<std::array<int, 4>> patterns;
    for (int mask = 0; mask < 256; ++mask) {
        const auto pattern = composeCellPattern(mask);
        auto found = std::find(patterns.begin(), patterns.end(), pattern);
        if (found == patterns.end()) {
            patterns_[mask] = static_cast<int>(patterns.size());
            patterns.push_back(pattern);
        } else {
            patterns_[mask] = static_cast<int>(found - patterns.begin());
        }
    }
    patternCount_ = static_cast<int>(patterns.size());
    const int maximumColumns =
        static_cast<int>(sf::Texture::getMaximumSize()) / cellSize;
    if (maximumColumns == 0) {
        throw std::runtime_error(
            "Autotile cell exceeds the maximum texture size");
    }
    const int count = patternCount_ * animationFrames_;
    const int columns = std::min(
        maximumColumns,
        static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))));
    const int pageCapacity = columns * maximumColumns;
    frames_.resize(count);
    for (int first = 0; first < count; first += pageCapacity) {
        const int pageCount = std::min(pageCapacity, count - first);
        const int rows = (pageCount + columns - 1) / columns;
        sf::Image page({static_cast<unsigned>(columns * cellSize),
                        static_cast<unsigned>(rows * cellSize)},
                       sf::Color::Transparent);
        for (int index = 0; index < pageCount; ++index) {
            const int animation = (first + index) / patternCount_;
            const auto& pattern = patterns[(first + index) % patternCount_];
            const sf::Vector2i origin{index % columns * cellSize,
                                      index / columns * cellSize};
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                const int half = cellSize / 2;
                const int cell = pattern[quadrant] - 1;
                const sf::Vector2i sourceOrigin{
                    animation * 3 * cellSize + cell % 3 * cellSize +
                        quadrant % 2 * half,
                    cell / 3 * cellSize + quadrant / 2 * half};
                if (!page.copy(original,
                               sf::Vector2u(origin +
                                            sf::Vector2i(quadrant % 2 * half,
                                                         quadrant / 2 * half)),
                               sf::IntRect(sourceOrigin, {half, half}),
                               false)) {
                    throw std::runtime_error(
                        "Failed to compose autotile material atlas");
                }
            }
            frames_[first + index].rectangle = {origin, {cellSize, cellSize}};
        }
        auto texture = std::make_shared<sf::Texture>(page);
        texture->setSmooth(false);
        for (int index = 0; index < pageCount; ++index) {
            frames_[first + index].texture = texture;
        }
    }
}

const AutoTileAtlas::Frame& AutoTileAtlas::frame(int mask,
                                                 int animationFrame) const {
    return frames_.at((animationFrame % animationFrames_) * patternCount_ +
                      patterns_.at(mask & 255));
}

}  // namespace ludork::engine::tilemap_graphics_impl
