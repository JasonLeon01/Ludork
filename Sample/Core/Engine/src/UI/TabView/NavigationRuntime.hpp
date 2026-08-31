#pragma once

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <optional>

namespace ludork::engine::tab_view_impl {

sf::Vector2f normalizedSize(const sf::Vector2f& size);
int clampedIndex(int index, std::size_t count);
std::optional<int> tabIndexAt(const sf::Vector2f& size,
                              const sf::Vector2f& localPosition,
                              std::size_t itemCount, float hintSize);
float contentWidth(float width, float hintSize);

}  // namespace ludork::engine::tab_view_impl
