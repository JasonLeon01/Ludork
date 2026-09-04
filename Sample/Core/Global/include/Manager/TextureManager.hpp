#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <GlobalRuntimeApi.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

BIND_CLASS()
class LUDORK_GLOBAL_API TextureManager {
public:
    BIND_METHOD(defaults = {false, nil, false})
    static std::shared_ptr<sf::Texture> load(
        const std::string& filePath, bool sRGB = false,
        std::optional<sf::IntRect> area = std::nullopt, bool smooth = false);

    BIND_METHOD()
    static std::size_t getMemory();

    static void clear() noexcept;

private:
    static std::string makeKey(const std::string& filePath, bool sRGB,
                               const std::optional<sf::IntRect>& area,
                               bool smooth);
};
