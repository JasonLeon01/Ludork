#include "Rendering/PixelConversion.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace ludork::preview_host {
namespace {

std::uint64_t checkedPixelByteCount(unsigned int width, unsigned int height,
                                    const std::string& source) {
    const std::uint64_t byteCount = static_cast<std::uint64_t>(width) *
                                    static_cast<std::uint64_t>(height) * 4u;
    if (byteCount >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::overflow_error(source + " is too large");
    }
    return byteCount;
}

}  // namespace

std::vector<std::uint8_t> bgraFromPremultipliedRgba(const sf::Image& image,
                                                    const sf::Vector2u& size) {
    const std::uint64_t byteCount =
        checkedPixelByteCount(size.x, size.y, "Preview frame");
    const std::uint8_t* source = image.getPixelsPtr();
    if (source == nullptr) {
        throw std::runtime_error("Preview renderer returned no pixels");
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(byteCount));
    for (std::size_t index = 0; index < result.size(); index += 4) {
        result[index] = source[index + 2];
        result[index + 1] = source[index + 1];
        result[index + 2] = source[index];
        result[index + 3] = source[index + 3];
    }
    return result;
}

std::vector<std::uint8_t> premultipliedBgraFromStraightRgba(
    const sf::Image& image, const sf::Vector2u& size) {
    const std::uint64_t byteCount =
        checkedPixelByteCount(size.x, size.y, "Preview frame");
    const std::uint8_t* source = image.getPixelsPtr();
    if (source == nullptr) {
        throw std::runtime_error("Preview renderer returned no pixels");
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(byteCount));
    for (std::size_t index = 0; index < result.size(); index += 4) {
        const std::uint32_t alpha = source[index + 3];
        result[index] = static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(source[index + 2]) * alpha + 127u) /
            255u);
        result[index + 1] = static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(source[index + 1]) * alpha + 127u) /
            255u);
        result[index + 2] = static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(source[index]) * alpha + 127u) / 255u);
        result[index + 3] = static_cast<std::uint8_t>(alpha);
    }
    return result;
}

}  // namespace ludork::preview_host
