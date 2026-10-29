#include <Manager/TextureManager.hpp>

#include "TextureManager/TextureManagerImpl.hpp"

std::shared_ptr<sf::Texture> TextureManager::load(
    const std::string& filePath, bool sRGB, std::optional<sf::IntRect> area,
    bool smooth) {
    return ludork::global::texture_manager_impl::textureManagerImpl()->load(
        filePath, sRGB, area, smooth);
}

std::shared_ptr<TextureLoadRequest> TextureManager::request(
    const std::string& filePath, bool sRGB, std::optional<sf::IntRect> area,
    bool smooth, int priority) {
    return ludork::global::texture_manager_impl::textureManagerImpl()->request(
        filePath, sRGB, area, smooth, priority);
}

std::size_t TextureManager::getMemory() {
    return ludork::global::texture_manager_impl::textureManagerImpl()->memory();
}

std::optional<std::string> TextureManager::getPath(
    const std::shared_ptr<sf::Texture>& texture) {
    return ludork::global::texture_manager_impl::textureManagerImpl()->path(
        texture);
}

void TextureManager::initialize() {
    ludork::global::texture_manager_impl::textureManagerImpl()->initialize();
}

void TextureManager::shutdown() noexcept {
    ludork::global::texture_manager_impl::textureManagerImpl()->shutdown();
}

void TextureManager::pumpUploads() {
    ludork::global::texture_manager_impl::textureManagerImpl()->pumpUploads();
}

TextureManager::Statistics TextureManager::getStatistics() {
    return ludork::global::texture_manager_impl::textureManagerImpl()
        ->statistics();
}

void TextureManager::clear() noexcept {
    shutdown();
}
