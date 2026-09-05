#include <Manager/FontManager.hpp>

#include <Runtime/AssetStore.hpp>
#include <Runtime/ConcurrentResourceCache.hpp>

#include <cstdint>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>

namespace {

struct FontResource {
    std::unique_ptr<ludork::runtime::AssetInputStream> stream;
    sf::Font font;
};

struct FontManagerState {
    ludork::runtime::ConcurrentResourceCache<sf::Font> resources;
    std::shared_mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<sf::Font>> fonts;
    std::unordered_map<std::string, std::string> filenames;
    std::unordered_map<std::string, std::string> familyByFilename;
    std::uint64_t generation = 0;
};

FontManagerState& fontManagerState() {
    static FontManagerState state;
    return state;
}

}  // namespace

std::shared_ptr<sf::Font> FontManager::load(const std::string& filePath) {
    FontManagerState& state = fontManagerState();
    std::uint64_t generation;
    {
        std::shared_lock lock(state.mutex);
        const auto filenameIterator = state.familyByFilename.find(filePath);
        if (filenameIterator != state.familyByFilename.end()) {
            return state.fonts.at(filenameIterator->second);
        }
        generation = state.generation;
    }
    const std::shared_ptr<sf::Font> font =
        state.resources.getOrLoad(filePath, [&]() {
            std::shared_ptr<FontResource> owner =
                std::make_shared<FontResource>();
            owner->stream = ludork::runtime::assetStore().open(filePath);
            if (!owner->font.openFromStream(*owner->stream)) {
                throw std::runtime_error("Failed to load font from file: " +
                                         filePath);
            }
            return std::shared_ptr<sf::Font>(owner, &owner->font);
        });
    const std::string family = font->getInfo().family;
    std::unique_lock lock(state.mutex);
    if (state.generation != generation) {
        return font;
    }
    const auto filenameIterator = state.familyByFilename.find(filePath);
    if (filenameIterator != state.familyByFilename.end()) {
        return state.fonts.at(filenameIterator->second);
    }
    auto updatedFonts = state.fonts;
    auto updatedFilenames = state.filenames;
    auto updatedFamilyByFilename = state.familyByFilename;
    updatedFonts[family] = font;
    updatedFilenames[family] = filePath;
    updatedFamilyByFilename[filePath] = family;
    state.fonts.swap(updatedFonts);
    state.filenames.swap(updatedFilenames);
    state.familyByFilename.swap(updatedFamilyByFilename);
    return font;
}

std::shared_ptr<sf::Font> FontManager::getFont(const std::string& fontName) {
    FontManagerState& state = fontManagerState();
    std::shared_lock lock(state.mutex);
    const auto iterator = state.fonts.find(fontName);
    if (iterator == state.fonts.end()) {
        throw std::out_of_range("Font " + fontName + " not found");
    }
    return iterator->second;
}

std::string FontManager::getFontFilename(const std::string& fontName) {
    FontManagerState& state = fontManagerState();
    {
        std::shared_lock lock(state.mutex);
        const auto iterator = state.filenames.find(fontName);
        if (iterator != state.filenames.end()) {
            return iterator->second;
        }
    }
    std::cerr << "Font " << fontName << " not found\n";
    return {};
}

std::vector<std::string> FontManager::getFontList() {
    FontManagerState& state = fontManagerState();
    std::shared_lock lock(state.mutex);
    std::vector<std::string> result;
    result.reserve(state.fonts.size());
    for (const auto& [fontName, font] : state.fonts) {
        static_cast<void>(font);
        result.push_back(fontName);
    }
    return result;
}

std::vector<std::string> FontManager::getFontFilenameList() {
    FontManagerState& state = fontManagerState();
    std::shared_lock lock(state.mutex);
    std::vector<std::string> result;
    result.reserve(state.filenames.size());
    for (const auto& [fontName, filePath] : state.filenames) {
        static_cast<void>(fontName);
        result.push_back(filePath);
    }
    return result;
}

bool FontManager::hasFont(const std::string& fontName) {
    FontManagerState& state = fontManagerState();
    std::shared_lock lock(state.mutex);
    return state.fonts.contains(fontName);
}

std::size_t FontManager::getMemory() {
    FontManagerState& state = fontManagerState();
    std::shared_lock lock(state.mutex);
    std::size_t total = sizeof(state) + state.resources.entryCount() *
                                            sizeof(std::weak_ptr<sf::Font>);
    for (const auto& [name, font] : state.fonts) {
        total += name.capacity() + sizeof(font) + sizeof(sf::Font);
    }
    for (const auto& [name, path] : state.filenames) {
        total += name.capacity() + path.capacity();
    }
    return total;
}

void FontManager::clear() noexcept {
    FontManagerState& state = fontManagerState();
    std::unique_lock lock(state.mutex);
    ++state.generation;
    state.resources.clear();
    state.familyByFilename.clear();
    state.filenames.clear();
    state.fonts.clear();
}
