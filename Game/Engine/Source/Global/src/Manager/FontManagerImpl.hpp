#pragma once
#include <Manager/FontManager.hpp>
#include <Runtime/AssetInputStream.hpp>
#include <Runtime/ConcurrentResourceCache.hpp>
#include <shared_mutex>
#include <unordered_map>
#include <cstdint>

struct FontManager::FontResource {
    std::unique_ptr<ludork::runtime::AssetInputStream> stream;
    sf::Font font;
};

struct FontManager::Impl {
    ludork::runtime::ConcurrentResourceCache<sf::Font> resources;
    std::shared_mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<sf::Font>> fonts;
    std::unordered_map<std::string, std::string> filenames;
    std::unordered_map<std::string, std::string> familyByFilename;
    std::uint64_t generation = 0;
};
