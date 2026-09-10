#include "FontManagerImpl.hpp"
#include <Manager/FontManager.hpp>
#include <Runtime/AssetInputStream.hpp>

#include <Runtime/AssetStore.hpp>
#include <Runtime/ConcurrentResourceCache.hpp>

#include <cstdint>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>

namespace {

FontManager::Impl& fontManagerImpl() {
    static FontManager::Impl impl;
    return impl;
}

}  // namespace

std::shared_ptr<sf::Font> FontManager::load(const std::string& filePath) {
    FontManager::Impl& impl = fontManagerImpl();
    std::uint64_t generation;
    {
        std::shared_lock lock(impl.mutex);
        const auto filenameIterator = impl.familyByFilename.find(filePath);
        if (filenameIterator != impl.familyByFilename.end()) {
            return impl.fonts.at(filenameIterator->second);
        }
        generation = impl.generation;
    }
    const std::shared_ptr<sf::Font> font =
        impl.resources.getOrLoad(filePath, [&]() {
            std::shared_ptr<FontManager::FontResource> owner =
                std::make_shared<FontManager::FontResource>();
            owner->stream = ludork::runtime::assetStore().open(filePath);
            if (!owner->font.openFromStream(*owner->stream)) {
                throw std::runtime_error("Failed to load font from file: " +
                                         filePath);
            }
            return std::shared_ptr<sf::Font>(owner, &owner->font);
        });
    const std::string family = font->getInfo().family;
    std::unique_lock lock(impl.mutex);
    if (impl.generation != generation) {
        return font;
    }
    const auto filenameIterator = impl.familyByFilename.find(filePath);
    if (filenameIterator != impl.familyByFilename.end()) {
        return impl.fonts.at(filenameIterator->second);
    }
    auto updatedFonts = impl.fonts;
    auto updatedFilenames = impl.filenames;
    auto updatedFamilyByFilename = impl.familyByFilename;
    updatedFonts[family] = font;
    updatedFilenames[family] = filePath;
    updatedFamilyByFilename[filePath] = family;
    impl.fonts.swap(updatedFonts);
    impl.filenames.swap(updatedFilenames);
    impl.familyByFilename.swap(updatedFamilyByFilename);
    return font;
}

std::shared_ptr<sf::Font> FontManager::getFont(const std::string& fontName) {
    FontManager::Impl& impl = fontManagerImpl();
    std::shared_lock lock(impl.mutex);
    const auto iterator = impl.fonts.find(fontName);
    if (iterator == impl.fonts.end()) {
        throw std::out_of_range("Font " + fontName + " not found");
    }
    return iterator->second;
}

std::string FontManager::getFontFilename(const std::string& fontName) {
    FontManager::Impl& impl = fontManagerImpl();
    {
        std::shared_lock lock(impl.mutex);
        const auto iterator = impl.filenames.find(fontName);
        if (iterator != impl.filenames.end()) {
            return iterator->second;
        }
    }
    std::cerr << "Font " << fontName << " not found\n";
    return {};
}

std::vector<std::string> FontManager::getFontList() {
    FontManager::Impl& impl = fontManagerImpl();
    std::shared_lock lock(impl.mutex);
    std::vector<std::string> result;
    result.reserve(impl.fonts.size());
    for (const auto& [fontName, font] : impl.fonts) {
        static_cast<void>(font);
        result.push_back(fontName);
    }
    return result;
}

std::vector<std::string> FontManager::getFontFilenameList() {
    FontManager::Impl& impl = fontManagerImpl();
    std::shared_lock lock(impl.mutex);
    std::vector<std::string> result;
    result.reserve(impl.filenames.size());
    for (const auto& [fontName, filePath] : impl.filenames) {
        static_cast<void>(fontName);
        result.push_back(filePath);
    }
    return result;
}

bool FontManager::hasFont(const std::string& fontName) {
    FontManager::Impl& impl = fontManagerImpl();
    std::shared_lock lock(impl.mutex);
    return impl.fonts.contains(fontName);
}

std::size_t FontManager::getMemory() {
    FontManager::Impl& impl = fontManagerImpl();
    std::shared_lock lock(impl.mutex);
    std::size_t total = sizeof(impl) + impl.resources.entryCount() *
                                           sizeof(std::weak_ptr<sf::Font>);
    for (const auto& [name, font] : impl.fonts) {
        total += name.capacity() + sizeof(font) + sizeof(sf::Font);
    }
    for (const auto& [name, path] : impl.filenames) {
        total += name.capacity() + path.capacity();
    }
    return total;
}

void FontManager::clear() noexcept {
    FontManager::Impl& impl = fontManagerImpl();
    std::unique_lock lock(impl.mutex);
    ++impl.generation;
    impl.resources.clear();
    impl.familyByFilename.clear();
    impl.filenames.clear();
    impl.fonts.clear();
}
