#include <AccelerateCalculation.hpp>

#include <stdexcept>

void C_ImageUpdateBuffer1D(sf::Texture& img, const std::string& buffer) {
    if (buffer.empty()) {
        throw std::invalid_argument("Expected a non-empty RGBA byte string");
    }
    img.update(reinterpret_cast<const std::uint8_t*>(buffer.data()));
}

void C_ImageUpdateBuffer3D(sf::Texture& img, const std::string& buffer) {
    C_ImageUpdateBuffer1D(img, buffer);
}
