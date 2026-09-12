#pragma once

#include <CoreMinimal.hpp>

#include <GlobalRuntimeApi.hpp>

BIND_CLASS()
class LUDORK_GLOBAL_API TextureManager {
public:
    BIND_METHOD(defaults = {false, nil, false})
    static std::shared_ptr<sf::Texture> load(
        const std::string& filePath, bool sRGB = false,
        std::optional<sf::IntRect> area = std::nullopt, bool smooth = false);

    BIND_METHOD()
    static std::size_t getMemory();

    /// Return the resource path associated with a cached texture, or nil for
    /// textures created outside this manager.
    BIND_METHOD(metadata = false)
    static std::optional<std::string> getPath(
        const std::shared_ptr<sf::Texture>& texture);

    static void clear() noexcept;

private:
    static std::string makeKey(const std::string& filePath, bool sRGB,
                               const std::optional<sf::IntRect>& area,
                               bool smooth);
};
