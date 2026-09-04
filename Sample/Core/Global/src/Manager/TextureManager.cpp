#include <Manager/TextureManager.hpp>

#include <Runtime/ConcurrentResourceCache.hpp>
#include <Utf8Path.hpp>

#include <sstream>
#include <stdexcept>
#include <string_view>

#ifndef LUDORK_PLATFORM
#define LUDORK_PLATFORM ""
#endif

namespace {

constexpr bool PinTextures = std::string_view(LUDORK_PLATFORM) == "ios";

ludork::runtime::ConcurrentResourceCache<sf::Texture, PinTextures>&
textureCache() {
    static ludork::runtime::ConcurrentResourceCache<sf::Texture, PinTextures>
        cache;
    return cache;
}

}  // namespace

std::shared_ptr<sf::Texture> TextureManager::load(
    const std::string& filePath, bool sRGB, std::optional<sf::IntRect> area,
    bool smooth) {
    const std::string key = makeKey(filePath, sRGB, area, smooth);
    return textureCache().getOrLoad(key, [&]() {
        auto texture = std::make_shared<sf::Texture>();
        const std::filesystem::path path =
            ludork::standard::pathFromUtf8(filePath);
        const bool loaded = area.has_value()
                                ? texture->loadFromFile(path, sRGB, *area)
                                : texture->loadFromFile(path, sRGB);
        if (!loaded) {
            throw std::runtime_error("Failed to load texture from file: " +
                                     filePath);
        }
        texture->setSmooth(smooth);
        return texture;
    });
}

std::size_t TextureManager::getMemory() {
    const std::size_t entries = textureCache().entryCount();
    return sizeof(textureCache()) +
           entries * (sizeof(sf::Texture) +
                      (PinTextures ? sizeof(std::shared_ptr<sf::Texture>)
                                   : sizeof(std::weak_ptr<sf::Texture>)));
}

void TextureManager::clear() noexcept {
    textureCache().clear();
}

std::string TextureManager::makeKey(const std::string& filePath, bool sRGB,
                                    const std::optional<sf::IntRect>& area,
                                    bool smooth) {
    std::ostringstream stream;
    stream << filePath << '\0' << sRGB << '\0';
    if (area.has_value()) {
        stream << area->position.x << ',' << area->position.y << ','
               << area->size.x << ',' << area->size.y;
    }
    stream << '\0' << smooth;
    return stream.str();
}
