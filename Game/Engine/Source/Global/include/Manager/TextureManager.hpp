#pragma once

#include <CoreMinimal.hpp>

#include <GlobalRuntimeApi.hpp>
#include <Manager/TextureLoadRequest.hpp>
#include <Manager/TextureLoadingStatistics.hpp>

BIND_CLASS()
class LUDORK_GLOBAL_API TextureManager {
public:
    using Statistics = TextureLoadingStatistics;

    BIND_METHOD(defaults = {false, nil, false})
    static std::shared_ptr<sf::Texture> load(
        const std::string& filePath, bool sRGB = false,
        std::optional<sf::IntRect> area = std::nullopt, bool smooth = false);

    /// Request a texture without waiting for file reading, decoding or upload.
    BIND_METHOD(metadata = false, defaults = {false, nil, false, 0},
                nonnull_return = true)
    static std::shared_ptr<TextureLoadRequest> request(
        const std::string& filePath, bool sRGB = false,
        std::optional<sf::IntRect> area = std::nullopt, bool smooth = false,
        int priority = 0);

    BIND_METHOD()
    static std::size_t getMemory();

    /// Return the resource path associated with a cached texture, or nil for
    /// textures created outside this manager.
    BIND_METHOD(metadata = false)
    static std::optional<std::string> getPath(
        const std::shared_ptr<sf::Texture>& texture);

    static void initialize();
    static void shutdown() noexcept;
    static void pumpUploads();
    static Statistics getStatistics();
    static void clear() noexcept;
};
