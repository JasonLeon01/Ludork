#pragma once

#include "Actor/ActorBatchRenderer.hpp"

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ludork::preview_host::actor_batch_detail {

struct FileStamp {
    bool exists = false;
    std::uint64_t size = 0;
    double modified = 0.0;

    bool operator==(const FileStamp&) const = default;
};

struct ActorVisualRequest {
    std::string id;
    std::string texturePath;
    sf::IntRect textureRect;
    std::string shaderPath;
    float hue = 0.0f;
};

}  // namespace ludork::preview_host::actor_batch_detail

namespace ludork::preview_host {

struct ActorBatchRenderer::Impl {
    void reset(const std::filesystem::path& projectPath);
    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);

private:
    struct PackedActorVisual {
        actor_batch_detail::ActorVisualRequest visual;
        std::size_t page = 0;
        sf::Vector2u position;
        bool shaderError = false;
        std::string error;
    };

    struct AtlasLayout {
        sf::Vector2u size;
        std::vector<std::size_t> visualIndices;
    };

    struct TextureCacheEntry {
        actor_batch_detail::FileStamp stamp;
        std::shared_ptr<sf::Texture> texture;
        std::string error;
    };

    struct ShaderCacheEntry {
        actor_batch_detail::FileStamp stamp;
        std::shared_ptr<sf::Shader> shader;
        std::string error;
    };

    PackedActorVisual parseVisual(const RuntimeData& value,
                                  std::size_t index) const;
    std::vector<AtlasLayout> pack(std::vector<PackedActorVisual>& visuals,
                                  unsigned int limit) const;
    std::vector<std::uint8_t> renderPage(
        const AtlasLayout& layout, std::vector<PackedActorVisual>& visuals,
        std::size_t pageIndex, float time);
    sf::RenderTexture& pageBuffer(const sf::Vector2u& size,
                                  std::size_t pageIndex);
    void renderVisual(sf::RenderTexture& target, PackedActorVisual& packed,
                      float time);
    const TextureCacheEntry& loadTexture(const std::string& path);
    const ShaderCacheEntry& loadShader(const std::string& path);
    sf::RenderTexture& effectBuffer(const sf::Vector2i& size);
    static std::uint64_t effectBufferKey(const sf::Vector2i& size);
    void pruneEffectBuffers(
        const std::unordered_set<std::uint64_t>& retainedKeys);
    sf::Shader& requireHueShader();
    sf::Shader& requireErrorShader();
    void drawError(sf::RenderTexture& target, const sf::Texture& texture,
                   const sf::IntRect& rect, const sf::Vector2u& position);
    static void bindActorShader(sf::Shader& shader, const sf::Texture& texture,
                                const sf::IntRect& rect, float time);
    static void bindHueShader(sf::Shader& shader, float hue);

    std::unordered_map<std::string, TextureCacheEntry> textures_;
    std::unordered_map<std::string, ShaderCacheEntry> shaders_;
    std::vector<std::unique_ptr<sf::RenderTexture>> pageBuffers_;
    std::unordered_map<std::uint64_t, std::unique_ptr<sf::RenderTexture>>
        effectBuffers_;
    std::unordered_set<std::uint64_t> activeEffectBufferKeys_;
    std::shared_ptr<sf::Shader> errorShader_;
};

}  // namespace ludork::preview_host
