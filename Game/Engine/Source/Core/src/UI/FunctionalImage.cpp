#include <UI/FunctionalImage.hpp>

FunctionalImage::FunctionalImage(std::shared_ptr<sf::Texture> texture,
                                 std::optional<sf::IntRect> rect)
    : Image(std::move(texture), rect), FunctionalBase() {}
