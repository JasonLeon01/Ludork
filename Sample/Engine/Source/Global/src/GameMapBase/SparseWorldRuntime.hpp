#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

namespace ludork::global::game_map_base_impl {

bool rectContains(const sf::IntRect& rect, const sf::Vector2i& position);
bool rectsIntersect(const sf::IntRect& left, const sf::IntRect& right);

}  // namespace ludork::global::game_map_base_impl
