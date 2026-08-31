#pragma once

#include <BindAnnotations.hpp>
#include <GlobalRuntimeApi.hpp>

#include <SFML/Graphics/Font.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

BIND_CLASS()
class LUDORK_GLOBAL_API FontManager {
public:
    BIND_METHOD()
    static std::shared_ptr<sf::Font> load(const std::string& filePath);

    BIND_METHOD()
    static std::shared_ptr<sf::Font> getFont(const std::string& fontName);

    BIND_METHOD()
    static std::string getFontFilename(const std::string& fontName);

    BIND_METHOD()
    static std::vector<std::string> getFontList();

    BIND_METHOD()
    static std::vector<std::string> getFontFilenameList();

    BIND_METHOD()
    static bool hasFont(const std::string& fontName);

    BIND_METHOD()
    static std::size_t getMemory();

    static void clear() noexcept;
};
