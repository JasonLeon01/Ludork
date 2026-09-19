#pragma once

#include <SFML/System/Vector2.hpp>
#include <optional>
#include <vector>

namespace ludork::global::game_map_base_impl {

class RegionVisibilityImpl {
public:
    void rebuild(const std::vector<std::vector<bool>>& passable);
    bool setObserver(std::optional<sf::Vector2i> position);
    bool isCellVisible(const sf::Vector2i& position) const;
    std::optional<sf::Vector2i> replacementSource(
        const sf::Vector2i& position) const;

private:
    bool contains(const sf::Vector2i& position) const;

    std::vector<std::vector<int>> regions_;
    std::vector<std::vector<std::optional<sf::Vector2i>>> replacementSources_;
    int observerRegion_ = -1;
};

}  // namespace ludork::global::game_map_base_impl
