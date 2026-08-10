#include <Manager/FontManager.hpp>

#include <Utf8Path.hpp>

#include <iostream>
#include <stdexcept>

std::unordered_map<std::string, std::shared_ptr<sf::Font>> FontManager::fonts_;
std::unordered_map<std::string, std::string> FontManager::filenames_;
std::unordered_map<std::string, std::string> FontManager::familyByFilename_;

std::shared_ptr<sf::Font> FontManager::load(const std::string& filePath) {
    const auto filenameIterator = familyByFilename_.find(filePath);
    if (filenameIterator != familyByFilename_.end()) {
        return fonts_.at(filenameIterator->second);
    }
    auto font = std::make_shared<sf::Font>();
    if (!font->openFromFile(ludork::standard::pathFromUtf8(filePath))) {
        throw std::runtime_error("Failed to load font from file: " + filePath);
    }
    const std::string family = font->getInfo().family;
    fonts_[family] = font;
    filenames_[family] = filePath;
    familyByFilename_[filePath] = family;
    return font;
}

std::shared_ptr<sf::Font> FontManager::getFont(const std::string& fontName) {
    const auto iterator = fonts_.find(fontName);
    if (iterator == fonts_.end()) {
        throw std::out_of_range("Font " + fontName + " not found");
    }
    return iterator->second;
}

std::string FontManager::getFontFilename(const std::string& fontName) {
    const auto iterator = filenames_.find(fontName);
    if (iterator != filenames_.end()) {
        return iterator->second;
    }
    std::cerr << "Font " << fontName << " not found\n";
    return {};
}

std::vector<std::string> FontManager::getFontList() {
    std::vector<std::string> result;
    result.reserve(fonts_.size());
    for (const auto& [fontName, font] : fonts_) {
        static_cast<void>(font);
        result.push_back(fontName);
    }
    return result;
}

std::vector<std::string> FontManager::getFontFilenameList() {
    std::vector<std::string> result;
    result.reserve(filenames_.size());
    for (const auto& [fontName, filePath] : filenames_) {
        static_cast<void>(fontName);
        result.push_back(filePath);
    }
    return result;
}

bool FontManager::hasFont(const std::string& fontName) {
    return fonts_.contains(fontName);
}

std::size_t FontManager::getMemory() {
    std::size_t total =
        sizeof(fonts_) + sizeof(filenames_) + sizeof(familyByFilename_);
    for (const auto& [name, font] : fonts_) {
        total += name.capacity() + sizeof(font) + sizeof(sf::Font);
    }
    for (const auto& [name, path] : filenames_) {
        total += name.capacity() + path.capacity();
    }
    return total;
}

void FontManager::clear() noexcept {
    familyByFilename_.clear();
    filenames_.clear();
    fonts_.clear();
}
