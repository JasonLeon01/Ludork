#include "Actor/ActorBatchRenderer.hpp"

#include "Protocol/FrameFiles.hpp"
#include "Protocol/PreviewProtocol.hpp"
#include "Rendering/PixelConversion.hpp"

#include <Runtime/AssetPath.hpp>
#include <Runtime/AssetStore.hpp>
#include <Runtime/RuntimeValueReader.hpp>
#include <Utf8Path.hpp>
#include <Utils/ShaderLoader.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::preview_host {
namespace {

constexpr unsigned int maximumAtlasSize = 2048;
constexpr unsigned int atlasGutter = 1;
constexpr float neutralHueEpsilon = 0.0001f;

struct FileStamp {
    bool exists = false;
    std::uint64_t size = 0;
    double modified = 0.0;

    bool operator==(const FileStamp&) const = default;
};

FileStamp fileStamp(const std::string& path) {
    const std::optional<ludork::runtime::AssetStat> stat =
        ludork::runtime::assetStore().stat(path);
    if (!stat.has_value() || stat->directory) {
        return {};
    }
    return {true, stat->size, stat->modificationTime};
}

std::string projectAssetPath(const std::string& value,
                             const std::string& source) {
    if (value.empty()) {
        throw std::invalid_argument(source + " cannot be empty");
    }
    static_cast<void>(ludork::runtime::AssetPath::parse(value));
    return value;
}

std::string actorTexturePath(const std::string& value) {
    return projectAssetPath(value, "Actor texturePath");
}

std::string actorShaderPath(const std::string& value) {
    return projectAssetPath(value, "Actor shaderPath");
}

FileStamp shaderStamp(const std::string& path) {
    const FileStamp requested = fileStamp(path);
    if (requested.exists) {
        return requested;
    }
    if (path.ends_with(".frag") || path.ends_with(".vert") ||
        path.ends_with(".geom")) {
        return fileStamp(path + 'c');
    }
    return requested;
}

struct ActorVisualRequest {
    std::string id;
    std::string texturePath;
    sf::IntRect textureRect;
    std::string shaderPath;
    float hue = 0.0f;
};

struct PackedActorVisual {
    ActorVisualRequest visual;
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
    FileStamp stamp;
    std::shared_ptr<sf::Texture> texture;
    std::string error;
};

struct ShaderCacheEntry {
    FileStamp stamp;
    std::shared_ptr<sf::Shader> shader;
    std::string error;
};

}  // namespace

struct ActorBatchRenderer::Impl {
    void reset(const std::filesystem::path& projectPath) {
        static_cast<void>(projectPath);
        textures_.clear();
        shaders_.clear();
        pageBuffers_.clear();
        effectBuffers_.clear();
        activeEffectBufferKeys_.clear();
        errorShader_.reset();
    }

    RuntimeData render(const RuntimeData::Map& request,
                       FrameFiles& frameFiles) {
        const std::int64_t generation =
            ludork::runtime::value_reader::requireInteger(
                ludork::runtime::value_reader::requireValue(
                    request, "generation", "Actor render request"),
                "Actor render request.generation");
        const float time = ludork::runtime::value_reader::requireFloat(
            ludork::runtime::value_reader::requireValue(request, "time",
                                                        "Actor render request"),
            "Actor render request.time");
        const RuntimeData::Array& itemValues =
            ludork::runtime::value_reader::requireArray(
                ludork::runtime::value_reader::requireValue(
                    request, "items", "Actor render request"),
                "Actor render request.items");
        std::vector<PackedActorVisual> visuals;
        visuals.reserve(itemValues.size());
        for (std::size_t index = 0; index < itemValues.size(); ++index) {
            visuals.push_back(parseVisual(itemValues[index], index));
        }
        std::stable_sort(
            visuals.begin(), visuals.end(),
            [](const PackedActorVisual& left, const PackedActorVisual& right) {
                return left.visual.id < right.visual.id;
            });
        const unsigned int limit =
            std::min(maximumAtlasSize, sf::Texture::getMaximumSize());
        if (limit <= atlasGutter * 2) {
            throw std::runtime_error(
                "OpenGL maximum texture size cannot hold an actor atlas");
        }
        std::vector<AtlasLayout> layouts = pack(visuals, limit);
        pageBuffers_.resize(layouts.size());
        std::unordered_set<std::uint64_t> requestedEffectBufferKeys;
        for (const PackedActorVisual& visual : visuals) {
            const bool hasHue =
                visual.visual.hue > neutralHueEpsilon &&
                std::abs(visual.visual.hue - 360.0f) > neutralHueEpsilon;
            if (!visual.visual.shaderPath.empty() && hasHue) {
                requestedEffectBufferKeys.insert(
                    effectBufferKey(visual.visual.textureRect.size));
            }
        }
        pruneEffectBuffers(requestedEffectBufferKeys);
        activeEffectBufferKeys_.clear();
        std::vector<std::vector<std::uint8_t>> pagePixels;
        pagePixels.reserve(layouts.size());
        for (std::size_t pageIndex = 0; pageIndex < layouts.size();
             ++pageIndex) {
            pagePixels.push_back(
                renderPage(layouts[pageIndex], visuals, pageIndex, time));
        }
        pruneEffectBuffers(activeEffectBufferKeys_);
        std::vector<std::uint8_t> sharedPixels;
        RuntimeData::Array pages;
        for (std::size_t pageIndex = 0; pageIndex < layouts.size();
             ++pageIndex) {
            const AtlasLayout& layout = layouts[pageIndex];
            const std::int64_t offset =
                static_cast<std::int64_t>(sharedPixels.size());
            const std::vector<std::uint8_t>& pixels = pagePixels[pageIndex];
            sharedPixels.insert(sharedPixels.end(), pixels.begin(),
                                pixels.end());
            pages.emplace_back(object({
                {"width",
                 RuntimeData(static_cast<std::int64_t>(layout.size.x))},
                {"height",
                 RuntimeData(static_cast<std::int64_t>(layout.size.y))},
                {"stride",
                 RuntimeData(static_cast<std::int64_t>(layout.size.x) * 4)},
                {"sharedMemory", RuntimeData(object({
                                     {"filePath", RuntimeData(std::string())},
                                     {"offset", RuntimeData(offset)},
                                 }))},
            }));
        }
        const std::filesystem::path& framePath = frameFiles.write(sharedPixels);
        for (RuntimeData& pageValue : pages) {
            RuntimeData::Map& page =
                *pageValue.getMutableIf<RuntimeData::Map>();
            RuntimeData::Map& sharedMemory =
                *page["sharedMemory"].getMutableIf<RuntimeData::Map>();
            sharedMemory["filePath"] =
                RuntimeData(ludork::standard::pathToUtf8(framePath));
        }
        RuntimeData::Array items;
        items.reserve(visuals.size());
        for (const PackedActorVisual& visual : visuals) {
            items.emplace_back(object({
                {"id", RuntimeData(visual.visual.id)},
                {"page", RuntimeData(static_cast<std::int64_t>(visual.page))},
                {"x",
                 RuntimeData(static_cast<std::int64_t>(visual.position.x))},
                {"y",
                 RuntimeData(static_cast<std::int64_t>(visual.position.y))},
                {"width", RuntimeData(static_cast<std::int64_t>(
                              visual.visual.textureRect.size.x))},
                {"height", RuntimeData(static_cast<std::int64_t>(
                               visual.visual.textureRect.size.y))},
                {"shaderError", RuntimeData(visual.shaderError)},
                {"error", RuntimeData(visual.error)},
            }));
        }
        return RuntimeData(object({
            {"type", RuntimeData("actorFrame")},
            {"generation", RuntimeData(generation)},
            {"pixelFormat", RuntimeData("Bgra8888Premultiplied")},
            {"pages", RuntimeData(std::move(pages))},
            {"items", RuntimeData(std::move(items))},
        }));
    }

private:
    PackedActorVisual parseVisual(const RuntimeData& value,
                                  std::size_t index) const {
        const std::string source =
            "Actor render request.items[" + std::to_string(index) + "]";
        const RuntimeData::Map& item =
            ludork::runtime::value_reader::requireMap(value, source);
        const RuntimeData::Map& rect =
            ludork::runtime::value_reader::requireMap(
                ludork::runtime::value_reader::requireValue(item, "textureRect",
                                                            source),
                source + ".textureRect");
        const int width = ludork::runtime::value_reader::requireInt(
            ludork::runtime::value_reader::requireValue(
                rect, "width", source + ".textureRect"),
            source + ".textureRect.width");
        const int height = ludork::runtime::value_reader::requireInt(
            ludork::runtime::value_reader::requireValue(
                rect, "height", source + ".textureRect"),
            source + ".textureRect.height");
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument(source +
                                        ".textureRect must have positive size");
        }
        const std::string& shaderText =
            ludork::runtime::value_reader::requireString(
                ludork::runtime::value_reader::requireValue(item, "shaderPath",
                                                            source),
                source + ".shaderPath");
        float hue = ludork::runtime::value_reader::requireFloat(
            ludork::runtime::value_reader::requireValue(item, "hue", source),
            source + ".hue");
        hue = std::fmod(hue, 360.0f);
        if (hue < 0.0f) {
            hue += 360.0f;
        }
        ActorVisualRequest visual{
            ludork::runtime::value_reader::requireString(
                ludork::runtime::value_reader::requireValue(item, "id", source),
                source + ".id"),
            actorTexturePath(ludork::runtime::value_reader::requireString(
                ludork::runtime::value_reader::requireValue(item, "texturePath",
                                                            source),
                source + ".texturePath")),
            sf::IntRect({ludork::runtime::value_reader::requireInt(
                             ludork::runtime::value_reader::requireValue(
                                 rect, "x", source + ".textureRect"),
                             source + ".textureRect.x"),
                         ludork::runtime::value_reader::requireInt(
                             ludork::runtime::value_reader::requireValue(
                                 rect, "y", source + ".textureRect"),
                             source + ".textureRect.y")},
                        {width, height}),
            shaderText.empty() ? std::string{} : actorShaderPath(shaderText),
            hue};
        return {std::move(visual)};
    }

    std::vector<AtlasLayout> pack(std::vector<PackedActorVisual>& visuals,
                                  unsigned int limit) const {
        std::vector<AtlasLayout> pages;
        if (visuals.empty()) {
            return pages;
        }
        pages.emplace_back();
        unsigned int x = atlasGutter;
        unsigned int y = atlasGutter;
        unsigned int rowHeight = 0;
        for (std::size_t index = 0; index < visuals.size(); ++index) {
            PackedActorVisual& visual = visuals[index];
            const unsigned int width =
                static_cast<unsigned int>(visual.visual.textureRect.size.x);
            const unsigned int height =
                static_cast<unsigned int>(visual.visual.textureRect.size.y);
            if (width + atlasGutter * 2 > limit ||
                height + atlasGutter * 2 > limit) {
                throw std::invalid_argument(
                    "Actor frame is larger than the maximum atlas page");
            }
            if (x + width + atlasGutter > limit) {
                x = atlasGutter;
                y += rowHeight + atlasGutter;
                rowHeight = 0;
            }
            if (y + height + atlasGutter > limit) {
                pages.emplace_back();
                x = atlasGutter;
                y = atlasGutter;
                rowHeight = 0;
            }
            AtlasLayout& page = pages.back();
            visual.page = pages.size() - 1;
            visual.position = {x, y};
            page.visualIndices.push_back(index);
            page.size.x = std::max(page.size.x, x + width + atlasGutter);
            page.size.y = std::max(page.size.y, y + height + atlasGutter);
            x += width + atlasGutter;
            rowHeight = std::max(rowHeight, height);
        }
        return pages;
    }

    std::vector<std::uint8_t> renderPage(
        const AtlasLayout& layout, std::vector<PackedActorVisual>& visuals,
        std::size_t pageIndex, float time) {
        sf::RenderTexture& target = pageBuffer(layout.size, pageIndex);
        target.setSmooth(false);
        target.clear(sf::Color::Transparent);
        for (const std::size_t index : layout.visualIndices) {
            renderVisual(target, visuals[index], time);
        }
        target.display();
        const sf::Image image = target.getTexture().copyToImage();
        return premultipliedBgraFromStraightRgba(image, layout.size);
    }

    sf::RenderTexture& pageBuffer(const sf::Vector2u& size,
                                  std::size_t pageIndex) {
        std::unique_ptr<sf::RenderTexture>& buffer = pageBuffers_[pageIndex];
        if (buffer == nullptr || buffer->getSize() != size) {
            buffer = std::make_unique<sf::RenderTexture>(size);
            buffer->setSmooth(false);
        }
        return *buffer;
    }

    void renderVisual(sf::RenderTexture& target, PackedActorVisual& packed,
                      float time) {
        const TextureCacheEntry& textureEntry =
            loadTexture(packed.visual.texturePath);
        if (textureEntry.texture == nullptr) {
            packed.error = textureEntry.error;
            return;
        }
        const sf::Vector2u textureSize = textureEntry.texture->getSize();
        const sf::IntRect& rect = packed.visual.textureRect;
        if (rect.position.x < 0 || rect.position.y < 0 ||
            static_cast<unsigned int>(rect.position.x + rect.size.x) >
                textureSize.x ||
            static_cast<unsigned int>(rect.position.y + rect.size.y) >
                textureSize.y) {
            packed.error = "Actor textureRect is outside the texture";
            return;
        }
        std::shared_ptr<sf::Shader> actorShader;
        if (!packed.visual.shaderPath.empty()) {
            const ShaderCacheEntry& shaderEntry =
                loadShader(packed.visual.shaderPath);
            actorShader = shaderEntry.shader;
            if (actorShader == nullptr) {
                packed.shaderError = true;
                packed.error = shaderEntry.error;
            }
        }
        const bool hasHue =
            packed.visual.hue > neutralHueEpsilon &&
            std::abs(packed.visual.hue - 360.0f) > neutralHueEpsilon;
        if (packed.shaderError) {
            drawError(target, *textureEntry.texture, rect, packed.position);
            return;
        }
        if (actorShader != nullptr && hasHue) {
            sf::RenderTexture& effect = effectBuffer(rect.size);
            effect.clear(sf::Color::Transparent);
            sf::Sprite source(*textureEntry.texture, rect);
            bindActorShader(*actorShader, *textureEntry.texture, rect, time);
            sf::RenderStates actorStates;
            actorStates.shader = actorShader.get();
            effect.draw(source, actorStates);
            effect.display();
            sf::Sprite composed(effect.getTexture());
            composed.setPosition(sf::Vector2f(packed.position));
            sf::Shader& hueShader = requireHueShader();
            bindHueShader(hueShader, packed.visual.hue);
            sf::RenderStates hueStates;
            hueStates.shader = &hueShader;
            target.draw(composed, hueStates);
            return;
        }
        sf::Sprite source(*textureEntry.texture, rect);
        source.setPosition(sf::Vector2f(packed.position));
        sf::RenderStates states(sf::BlendNone);
        if (actorShader != nullptr) {
            bindActorShader(*actorShader, *textureEntry.texture, rect, time);
            states.shader = actorShader.get();
        } else if (hasHue) {
            sf::Shader& hueShader = requireHueShader();
            bindHueShader(hueShader, packed.visual.hue);
            states.shader = &hueShader;
        }
        target.draw(source, states);
    }

    const TextureCacheEntry& loadTexture(const std::string& path) {
        const std::string& key = path;
        const FileStamp stamp = fileStamp(key);
        const auto iterator = textures_.find(key);
        if (iterator != textures_.end() && iterator->second.stamp == stamp) {
            return iterator->second;
        }
        TextureCacheEntry entry;
        entry.stamp = stamp;
        if (!stamp.exists) {
            entry.error = "Texture file not found: " + key;
        } else {
            std::unique_ptr<ludork::runtime::AssetInputStream> stream =
                ludork::runtime::assetStore().open(key);
            entry.texture = std::make_shared<sf::Texture>();
            if (!entry.texture->loadFromStream(*stream, false)) {
                entry.texture.reset();
                entry.error = "Texture load failed: " + key;
            } else {
                entry.texture->setSmooth(false);
            }
        }
        return textures_.insert_or_assign(key, std::move(entry)).first->second;
    }

    const ShaderCacheEntry& loadShader(const std::string& path) {
        const std::string& key = path;
        const FileStamp stamp = shaderStamp(path);
        const auto iterator = shaders_.find(key);
        if (iterator != shaders_.end() && iterator->second.stamp == stamp) {
            return iterator->second;
        }
        ShaderCacheEntry entry;
        entry.stamp = stamp;
        const ShaderLoadResult result =
            ShaderLoader::load(key, sf::Shader::Type::Fragment);
        entry.shader = result.shader;
        entry.error = result.error;
        return shaders_.insert_or_assign(key, std::move(entry)).first->second;
    }

    sf::RenderTexture& effectBuffer(const sf::Vector2i& size) {
        const std::uint64_t key = effectBufferKey(size);
        activeEffectBufferKeys_.insert(key);
        const auto iterator = effectBuffers_.find(key);
        if (iterator != effectBuffers_.end()) {
            return *iterator->second;
        }
        std::unique_ptr<sf::RenderTexture> buffer =
            std::make_unique<sf::RenderTexture>(
                sf::Vector2u(static_cast<unsigned int>(size.x),
                             static_cast<unsigned int>(size.y)));
        buffer->setSmooth(false);
        return *effectBuffers_.emplace(key, std::move(buffer)).first->second;
    }

    static std::uint64_t effectBufferKey(const sf::Vector2i& size) {
        return static_cast<std::uint64_t>(static_cast<unsigned int>(size.x))
                   << 32u |
               static_cast<unsigned int>(size.y);
    }

    void pruneEffectBuffers(
        const std::unordered_set<std::uint64_t>& retainedKeys) {
        for (auto iterator = effectBuffers_.begin();
             iterator != effectBuffers_.end();) {
            if (!retainedKeys.contains(iterator->first)) {
                iterator = effectBuffers_.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }

    sf::Shader& requireHueShader() {
        const std::string path =
            actorShaderPath("/Game/Assets/Shaders/Global/Hue.frag");
        const ShaderCacheEntry& entry = loadShader(path);
        if (entry.shader == nullptr) {
            throw std::runtime_error(entry.error);
        }
        return *entry.shader;
    }

    sf::Shader& requireErrorShader() {
        if (errorShader_ == nullptr) {
            constexpr const char* source = R"(
#ifdef GL_ES
precision highp float;
varying mediump vec4 sf_FrontColor;
varying mediump vec4 sf_TexCoord0;
#else
varying vec4 sf_FrontColor;
varying vec4 sf_TexCoord0;
#endif
uniform sampler2D texture;
void main()
{
    vec4 source = texture2D(texture, sf_TexCoord0.xy);
    gl_FragColor = vec4(source.r, 0.0, source.b, source.a) * sf_FrontColor;
}
)";
            errorShader_ = std::make_shared<sf::Shader>();
            if (!errorShader_->loadFromMemory(source,
                                              sf::Shader::Type::Fragment)) {
                errorShader_.reset();
                throw std::runtime_error(
                    "Failed to compile actor shader error renderer");
            }
        }
        return *errorShader_;
    }

    void drawError(sf::RenderTexture& target, const sf::Texture& texture,
                   const sf::IntRect& rect, const sf::Vector2u& position) {
        sf::Sprite source(texture, rect);
        source.setPosition(sf::Vector2f(position));
        sf::Shader& shader = requireErrorShader();
        shader.setUniform("texture", sf::Shader::CurrentTexture);
        sf::RenderStates states(sf::BlendNone);
        states.shader = &shader;
        target.draw(source, states);
    }

    static void bindActorShader(sf::Shader& shader, const sf::Texture& texture,
                                const sf::IntRect& rect, float time) {
        const sf::Vector2u size = texture.getSize();
        shader.setUniform("texture", sf::Shader::CurrentTexture);
        shader.setUniform("time", time);
        shader.setUniform("textureSize",
                          sf::Vector2f(static_cast<float>(size.x),
                                       static_cast<float>(size.y)));
        shader.setUniform("textureRect",
                          sf::Glsl::Vec4(static_cast<float>(rect.position.x),
                                         static_cast<float>(rect.position.y),
                                         static_cast<float>(rect.size.x),
                                         static_cast<float>(rect.size.y)));
    }

    static void bindHueShader(sf::Shader& shader, float hue) {
        shader.setUniform("screenTex", sf::Shader::CurrentTexture);
        shader.setUniform("hue", hue);
    }

    std::unordered_map<std::string, TextureCacheEntry> textures_;
    std::unordered_map<std::string, ShaderCacheEntry> shaders_;
    std::vector<std::unique_ptr<sf::RenderTexture>> pageBuffers_;
    std::unordered_map<std::uint64_t, std::unique_ptr<sf::RenderTexture>>
        effectBuffers_;
    std::unordered_set<std::uint64_t> activeEffectBufferKeys_;
    std::shared_ptr<sf::Shader> errorShader_;
};

ActorBatchRenderer::ActorBatchRenderer() : impl_(std::make_unique<Impl>()) {}

ActorBatchRenderer::~ActorBatchRenderer() = default;

void ActorBatchRenderer::reset(const std::filesystem::path& projectPath) {
    impl_->reset(projectPath);
}

RuntimeData ActorBatchRenderer::render(const RuntimeData::Map& request,
                                       FrameFiles& frameFiles) {
    return impl_->render(request, frameFiles);
}

}  // namespace ludork::preview_host
