#include <Manager/TextureManager.hpp>

#include <Utf8Path.hpp>

#include <sstream>
#include <stdexcept>
#include <string_view>

#ifndef LUDORK_PLATFORM
#define LUDORK_PLATFORM ""
#endif

std::unordered_map<std::string, std::weak_ptr<sf::Texture>>
    TextureManager::textures_;
std::unordered_map<std::string, std::shared_ptr<sf::Texture>>
    TextureManager::pinnedTextures_;

std::shared_ptr<sf::Texture> TextureManager::load(
    const std::string& filePath, bool sRGB, std::optional<sf::IntRect> area,
    bool smooth) {
    const std::string key = makeKey(filePath, sRGB, area, smooth);
    if (std::string_view(LUDORK_PLATFORM) == "ios") {
        const auto iterator = pinnedTextures_.find(key);
        if (iterator != pinnedTextures_.end()) {
            return iterator->second;
        }
    } else {
        const auto iterator = textures_.find(key);
        if (iterator != textures_.end()) {
            const std::shared_ptr<sf::Texture> texture =
                iterator->second.lock();
            if (texture != nullptr) {
                return texture;
            }
        }
    }
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
    if (std::string_view(LUDORK_PLATFORM) == "ios") {
        pinnedTextures_[key] = texture;
    } else {
        textures_[key] = texture;
    }
    return texture;
}

std::size_t TextureManager::getMemory() {
    removeExpired();
    return sizeof(textures_) + sizeof(pinnedTextures_) +
           textures_.size() *
               (sizeof(sf::Texture) + sizeof(std::weak_ptr<sf::Texture>)) +
           pinnedTextures_.size() *
               (sizeof(sf::Texture) + sizeof(std::shared_ptr<sf::Texture>));
}

void TextureManager::clear() noexcept {
    pinnedTextures_.clear();
    textures_.clear();
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

void TextureManager::removeExpired() {
    for (auto iterator = textures_.begin(); iterator != textures_.end();) {
        if (iterator->second.expired()) {
            iterator = textures_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}
