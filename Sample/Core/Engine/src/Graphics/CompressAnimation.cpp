#include <Graphics/CompressAnimation.hpp>

#include <Base64.hpp>
#include <Utf8Path.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Angle.hpp>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace {

struct SegmentTransform {
    float x;
    float y;
    float rotation;
    float scaleX;
    float scaleY;
};

std::optional<SegmentTransform> getSegmentTransform(
    const AnimationSegment& segment, float frameTime) {
    if (frameTime < segment.startFrame.time ||
        frameTime > segment.endFrame.time) {
        return std::nullopt;
    }
    const float duration = segment.endFrame.time - segment.startFrame.time;
    const float factor = duration <= 1e-4f
                             ? 0.0f
                             : (frameTime - segment.startFrame.time) / duration;
    return SegmentTransform{
        segment.startFrame.position[0] +
            (segment.endFrame.position[0] - segment.startFrame.position[0]) *
                factor,
        segment.startFrame.position[1] +
            (segment.endFrame.position[1] - segment.startFrame.position[1]) *
                factor,
        segment.startFrame.rotation +
            (segment.endFrame.rotation - segment.startFrame.rotation) * factor,
        segment.startFrame.scale[0] +
            (segment.endFrame.scale[0] - segment.startFrame.scale[0]) * factor,
        segment.startFrame.scale[1] +
            (segment.endFrame.scale[1] - segment.startFrame.scale[1]) * factor};
}

std::pair<float, float> getRotatedSize(float width, float height,
                                       float rotation) {
    const float radians = rotation * std::numbers::pi_v<float> / 180.0f;
    const float cosine = std::abs(std::cos(radians));
    const float sine = std::abs(std::sin(radians));
    return {width * cosine + height * sine, width * sine + height * cosine};
}

sf::Texture* getTexture(
    std::unordered_map<int, std::unique_ptr<sf::Texture>>& cache,
    const std::vector<std::string>& assets, const std::filesystem::path& root,
    int index) {
    const auto existing = cache.find(index);
    if (existing != cache.end()) {
        return existing->second.get();
    }
    if (index < 0 || index >= static_cast<int>(assets.size())) {
        return nullptr;
    }
    const std::filesystem::path path =
        root /
        ludork::standard::pathFromUtf8(assets[static_cast<std::size_t>(index)]);
    std::unique_ptr<sf::Texture> texture = std::make_unique<sf::Texture>(path);
    sf::Texture* result = texture.get();
    cache.emplace(index, std::move(texture));
    return result;
}

std::vector<std::uint8_t> compressFrame(const std::uint8_t* data,
                                        std::size_t size) {
    uLongf compressedSize = compressBound(static_cast<uLong>(size));
    std::vector<std::uint8_t> compressed(compressedSize);
    const int result = compress2(compressed.data(), &compressedSize, data,
                                 static_cast<uLong>(size), Z_BEST_COMPRESSION);
    if (result != Z_OK) {
        throw std::runtime_error("Failed to compress animation frame");
    }
    compressed.resize(compressedSize);
    return compressed;
}

void updateCanvasExtent(
    const std::vector<AnimationTimeline>& timeLines, float frameTime,
    const std::vector<std::string>& assets, const std::filesystem::path& root,
    std::unordered_map<int, std::unique_ptr<sf::Texture>>& cache, float& maxX,
    float& maxY) {
    for (const AnimationTimeline& timeline : timeLines) {
        for (const AnimationSegment& segment : timeline.timeSegments) {
            if (segment.type != "frame") {
                continue;
            }
            sf::Texture* texture =
                getTexture(cache, assets, root, segment.asset);
            const std::optional<SegmentTransform> transform =
                getSegmentTransform(segment, frameTime);
            if (texture == nullptr || !transform.has_value()) {
                continue;
            }
            const sf::Vector2u size = texture->getSize();
            const auto [width, height] = getRotatedSize(
                static_cast<float>(size.x) * std::abs(transform->scaleX),
                static_cast<float>(size.y) * std::abs(transform->scaleY),
                transform->rotation);
            maxX = std::max(maxX, std::abs(transform->x - width / 2.0f));
            maxX = std::max(maxX, std::abs(transform->x + width / 2.0f));
            maxY = std::max(maxY, std::abs(transform->y - height / 2.0f));
            maxY = std::max(maxY, std::abs(transform->y + height / 2.0f));
        }
    }
}

struct CompressedAnimationFrames {
    float duration = 0.0f;
    std::vector<std::string> frames;
    std::vector<AnimationSoundEntry> sounds;
};

std::vector<AnimationTimeTag> sortedTimeTags(
    const std::vector<AnimationTimeTag>& source) {
    std::vector<AnimationTimeTag> result = source;
    std::stable_sort(
        result.begin(), result.end(),
        [](const AnimationTimeTag& left, const AnimationTimeTag& right) {
            return left.time < right.time;
        });
    return result;
}

CompressedAnimationFrames compressAnimationFrames(
    int frameCount, float frameStep, int frameRate,
    const std::vector<AnimationTimeline>& timeLines,
    const std::vector<std::string>& assets,
    const std::filesystem::path& assetsRoot, const std::string& imageFormat) {
    CompressedAnimationFrames result;
    result.duration = frameRate > 0 ? static_cast<float>(frameCount) /
                                          static_cast<float>(frameRate)
                                    : 0.0f;

    std::unordered_map<int, std::unique_ptr<sf::Texture>> cache;
    float maxX = 0.0f;
    float maxY = 0.0f;
    for (int frame = 0; frame < frameCount; ++frame) {
        updateCanvasExtent(timeLines, frame * frameStep, assets, assetsRoot,
                           cache, maxX, maxY);
    }
    const unsigned int width =
        std::max(1u, static_cast<unsigned int>(std::ceil(maxX * 2.0f)));
    const unsigned int height =
        std::max(1u, static_cast<unsigned int>(std::ceil(maxY * 2.0f)));
    sf::RenderTexture target({width, height});
    if (!target.setActive(true)) {
        throw std::runtime_error("Failed to activate animation render texture");
    }

    if (frameCount > 0) {
        result.frames.reserve(static_cast<std::size_t>(frameCount));
    }
    for (int frame = 0; frame < frameCount; ++frame) {
        target.clear(sf::Color::Transparent);
        const float frameTime = frame * frameStep;
        for (const AnimationTimeline& timeline : timeLines) {
            for (const AnimationSegment& segment : timeline.timeSegments) {
                if (segment.type != "frame") {
                    continue;
                }
                sf::Texture* texture =
                    getTexture(cache, assets, assetsRoot, segment.asset);
                const std::optional<SegmentTransform> transform =
                    getSegmentTransform(segment, frameTime);
                if (texture == nullptr || !transform.has_value()) {
                    continue;
                }
                sf::Sprite sprite(*texture);
                const sf::Vector2u size = texture->getSize();
                sprite.setOrigin({static_cast<float>(size.x) / 2.0f,
                                  static_cast<float>(size.y) / 2.0f});
                sprite.setPosition(
                    {transform->x + static_cast<float>(width) / 2.0f,
                     transform->y + static_cast<float>(height) / 2.0f});
                sprite.setRotation(sf::degrees(transform->rotation));
                sprite.setScale(
                    {transform->scaleX * (segment.flipX ? -1.0f : 1.0f),
                     transform->scaleY});
                target.draw(sprite);
            }
        }
        target.display();
        const std::optional<std::vector<std::uint8_t>> memory =
            target.getTexture().copyToImage().saveToMemory(imageFormat);
        if (!memory.has_value()) {
            throw std::runtime_error("Failed to encode animation frame");
        }
        const std::vector<std::uint8_t> compressed =
            compressFrame(memory->data(), memory->size());
        result.frames.emplace_back(
            reinterpret_cast<const char*>(compressed.data()),
            compressed.size());
    }

    for (const AnimationTimeline& timeline : timeLines) {
        for (const AnimationSegment& segment : timeline.timeSegments) {
            if (segment.type != "sound") {
                continue;
            }
            AnimationSoundEntry entry;
            if (segment.asset >= 0 &&
                segment.asset < static_cast<int>(assets.size())) {
                entry.asset = assets[static_cast<std::size_t>(segment.asset)];
            }
            entry.startFrame =
                std::max(0, static_cast<int>(std::floor(
                                segment.startFrame.time * frameRate + 1e-5f)));
            entry.endFrame = std::max(
                0, static_cast<int>(
                       std::ceil(segment.endFrame.time * frameRate - 1e-5f)));
            entry.originalDuration = segment.originalDuration;
            entry.stopAtEndFrame =
                segment.originalDuration.has_value() &&
                *segment.originalDuration > 0.0f &&
                segment.endFrame.time - segment.startFrame.time + 1e-5f <
                    *segment.originalDuration;
            result.sounds.push_back(std::move(entry));
        }
    }
    return result;
}

}  // namespace

std::tuple<float, std::vector<std::string>, std::vector<AnimationSoundEntry>>
C_CompressAnimation(const sol::object& zlibModule, int frameCount,
                    float frameStep, int frameRate,
                    const std::vector<AnimationTimeline>& timeLines,
                    const std::vector<std::string>& assets,
                    const std::string& assetsRoot,
                    const std::string& imageFormat) {
    (void)zlibModule;
    CompressedAnimationFrames result = compressAnimationFrames(
        frameCount, frameStep, frameRate, timeLines, assets,
        ludork::standard::pathFromUtf8(assetsRoot), imageFormat);
    return {result.duration, std::move(result.frames),
            std::move(result.sounds)};
}

AnimationData compressAnimation(std::optional<AnimationSourceData> sourceValue,
                                const std::string& assetsRoot,
                                const std::string& imageFormat) {
    if (!sourceValue.has_value() ||
        (sourceValue->type.empty() && sourceValue->name.empty() &&
         sourceValue->frameRate == 0 && sourceValue->frameCount == 0 &&
         !sourceValue->visualFrameCount.has_value() &&
         !sourceValue->duration.has_value() &&
         !sourceValue->visualDuration.has_value() &&
         sourceValue->timeLines.empty() && sourceValue->assets.empty())) {
        AnimationData empty;
        empty.frameRate = 0;
        empty.frameCount = 0;
        empty.visualFrameCount = 0;
        empty.duration = 0.0f;
        empty.visualDuration = 0.0f;
        if (sourceValue.has_value()) {
            empty.timeTags = sortedTimeTags(sourceValue->timeTags);
        }
        return empty;
    }

    const AnimationSourceData& source = *sourceValue;
    AnimationData result;
    result.name = source.name;
    result.frameRate = source.frameRate > 0 ? source.frameRate : 30;
    result.timeTags = sortedTimeTags(source.timeTags);

    float maxTime = 0.0f;
    float visualMaxTime = 0.0f;
    for (const AnimationTimeline& timeline : source.timeLines) {
        for (const AnimationSegment& segment : timeline.timeSegments) {
            maxTime = std::max(maxTime, segment.endFrame.time);
            if (segment.type == "frame") {
                visualMaxTime = std::max(visualMaxTime, segment.endFrame.time);
            }
        }
    }
    result.frameCount =
        std::max(1, static_cast<int>(std::ceil(maxTime * result.frameRate)));
    const int visualFrameCount =
        visualMaxTime > 0.0f
            ? std::max(1, static_cast<int>(
                              std::ceil(visualMaxTime * result.frameRate)))
            : 0;
    result.visualFrameCount = visualFrameCount;
    result.duration = static_cast<float>(result.frameCount) / result.frameRate;
    result.visualDuration =
        static_cast<float>(visualFrameCount) / result.frameRate;

    const float frameStep = 1.0f / result.frameRate;
    CompressedAnimationFrames compressed = compressAnimationFrames(
        result.frameCount, frameStep, result.frameRate, source.timeLines,
        source.assets, ludork::standard::pathFromUtf8(assetsRoot), imageFormat);
    result.duration = compressed.duration;
    result.frames.reserve(compressed.frames.size());
    for (const std::string& frame : compressed.frames) {
        result.frames.emplace_back(
            ludork::standard::encodeBase64(std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(frame.data()),
                frame.size())));
    }
    result.sounds = std::move(compressed.sounds);
    return result;
}
