#include <Animation.hpp>

#include <Base64.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {
std::unique_ptr<sf::Texture>& placeholderTextureStorage() {
    static std::unique_ptr<sf::Texture> texture;
    return texture;
}

const sf::Texture& placeholderTexture() {
    std::unique_ptr<sf::Texture>& texture = placeholderTextureStorage();
    if (texture == nullptr) {
        texture = std::make_unique<sf::Texture>(
            sf::Image({1u, 1u}, sf::Color::Transparent));
    }
    return *texture;
}

std::vector<std::uint8_t> decodeBase64(const std::string& data) {
    try {
        return ludork::standard::decodeBase64(data);
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument("Invalid base64 animation frame");
    }
}

std::vector<std::uint8_t> decompressFrame(const std::uint8_t* data,
                                          std::size_t size) {
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(data);
    stream.avail_in = static_cast<uInt>(size);
    if (inflateInit(&stream) != Z_OK) {
        throw std::runtime_error(
            "Failed to initialise animation decompression");
    }
    std::vector<std::uint8_t> output;
    std::array<std::uint8_t, 16384> chunk{};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = chunk.data();
        stream.avail_out = static_cast<uInt>(chunk.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const std::size_t produced = chunk.size() - stream.avail_out;
        output.insert(output.end(), chunk.begin(), chunk.begin() + produced);
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END) {
        throw std::runtime_error("Failed to decompress animation frame");
    }
    return output;
}

void resolveSoundStopAtEndFrame(AnimationSoundEntry& entry, int frameRate) {
    if (entry.stopAtEndFrame.has_value()) {
        return;
    }
    if (!entry.originalDuration.has_value() ||
        *entry.originalDuration <= 0.0f || entry.endFrame < 0 ||
        frameRate <= 0) {
        entry.stopAtEndFrame = false;
        return;
    }
    const float effectiveDuration =
        static_cast<float>(entry.endFrame - entry.startFrame) / frameRate;
    entry.stopAtEndFrame = effectiveDuration + 1e-5f < *entry.originalDuration;
}

float getCompressedAnimationVisualDuration(const AnimationData& animationData) {
    if (animationData.visualDuration.has_value()) {
        return *animationData.visualDuration;
    }
    const int frameRate =
        animationData.frameRate > 0 ? animationData.frameRate : 30;
    if (animationData.visualFrameCount.has_value()) {
        return static_cast<float>(*animationData.visualFrameCount) / frameRate;
    }
    if (animationData.duration.has_value()) {
        return *animationData.duration;
    }
    if (animationData.frameCount > 0) {
        return static_cast<float>(animationData.frameCount) / frameRate;
    }
    return 0.0f;
}
}  // namespace

AnimSprite::AnimSprite(const AnimationData& animationData)
    : sf::Sprite(placeholderTexture()) {
    setData(animationData);
}

void AnimSprite::setData(const AnimationData& animationData) {
    frames_ = animationData.frames;
    frameEncoding_ = animationData.frameEncoding.empty()
                         ? "zlib"
                         : animationData.frameEncoding;
    frameRate_ = animationData.frameRate > 0 ? animationData.frameRate : 30;
    frameCount_ = animationData.frameCount > 0
                      ? animationData.frameCount
                      : static_cast<int>(frames_.size());
    if (frameCount_ <= 0) {
        frameCount_ = static_cast<int>(frames_.size());
    }
    soundEntries_ = animationData.sounds;
    timeTags_ = animationData.timeTags;
    std::stable_sort(
        timeTags_.begin(), timeTags_.end(),
        [](const AnimationTimeTag& left, const AnimationTimeTag& right) {
            return left.time < right.time;
        });
    std::stable_sort(
        soundEntries_.begin(), soundEntries_.end(),
        [](const AnimationSoundEntry& left, const AnimationSoundEntry& right) {
            return left.startFrame < right.startFrame;
        });
    for (AnimationSoundEntry& entry : soundEntries_) {
        resolveSoundStopAtEndFrame(entry, frameRate_);
    }
    frameCounter_ = 0.0f;
    frameIndex_ = 0;
    finished_ = frameCount_ <= 0 || frames_.empty();
    duration_ = animationData.duration.value_or(0.0f);
    if (duration_ <= 0.0f && frameRate_ > 0 && frameCount_ > 0) {
        duration_ = static_cast<float>(frameCount_) / frameRate_;
    }
    visualDuration_ = getCompressedAnimationVisualDuration(animationData);
    if (!finished_) {
        applyFrame(0);
    }
}

float AnimSprite::getDuration() const {
    return duration_;
}

float AnimSprite::getVisualDuration() const {
    return visualDuration_;
}

std::vector<AnimationTimeTag> AnimSprite::getAllTimeTags() const {
    return timeTags_;
}

bool AnimSprite::isFinished() const {
    return finished_;
}

int AnimSprite::getFrameIndex() const {
    return frameIndex_;
}

void AnimSprite::update(float deltaTime) {
    if (finished_) {
        return;
    }
    if (frameCount_ <= 0) {
        finished_ = true;
        return;
    }
    if (deltaTime > 0.0f) {
        frameCounter_ += deltaTime * frameRate_;
    }
    int newFrameIndex = static_cast<int>(std::floor(frameCounter_));
    if (newFrameIndex >= frameCount_) {
        finished_ = true;
        newFrameIndex = frameCount_ - 1;
    }
    if (newFrameIndex == frameIndex_) {
        return;
    }
    frameIndex_ = newFrameIndex;
    applyFrame(frameIndex_);
}

void AnimSprite::applyFrame(int frameIndex) {
    if (frameIndex < 0 || frameIndex >= static_cast<int>(frames_.size())) {
        return;
    }
    auto& frame = frames_[static_cast<std::size_t>(frameIndex)];
    std::shared_ptr<sf::Image> image;
    if (std::holds_alternative<std::shared_ptr<sf::Image>>(frame)) {
        image = std::get<std::shared_ptr<sf::Image>>(frame);
    } else {
        const std::string& encoded = std::get<std::string>(frame);
        std::vector<std::uint8_t> compressed;
        if (frameEncoding_ == "base64+zlib") {
            compressed = decodeBase64(encoded);
        } else {
            compressed.assign(encoded.begin(), encoded.end());
        }
        const std::vector<std::uint8_t> memory =
            decompressFrame(compressed.data(), compressed.size());
        image = std::make_shared<sf::Image>();
        if (!image->loadFromMemory(memory.data(), memory.size())) {
            return;
        }
        frame = image;
    }
    if (!image) {
        return;
    }
    if (!texture_) {
        texture_ = std::make_shared<sf::Texture>(*image);
        sf::Sprite::setTexture(*texture_, true);
    } else {
        texture_->update(*image);
    }
}

const std::vector<AnimationSoundEntry>& AnimSprite::getSoundEntries() const {
    return soundEntries_;
}

std::vector<AnimationSoundEntry>& AnimSprite::getSoundEntries() {
    return soundEntries_;
}

float getAnimationVisualDuration(const AnimationSourceData& animationData) {
    if (animationData.visualDuration.has_value()) {
        return *animationData.visualDuration;
    }
    const int frameRate =
        animationData.frameRate > 0 ? animationData.frameRate : 30;
    if (animationData.visualFrameCount.has_value()) {
        return static_cast<float>(*animationData.visualFrameCount) / frameRate;
    }
    if (animationData.type == "animation") {
        float visualMaxTime = 0.0f;
        for (const AnimationTimeline& timeline : animationData.timeLines) {
            for (const AnimationSegment& segment : timeline.timeSegments) {
                if (segment.type == "frame") {
                    visualMaxTime =
                        std::max(visualMaxTime, segment.endFrame.time);
                }
            }
        }
        if (visualMaxTime <= 0.0f) {
            return 0.0f;
        }
        return static_cast<float>(std::max(
                   1, static_cast<int>(std::ceil(visualMaxTime * frameRate)))) /
               frameRate;
    }
    if (animationData.duration.has_value()) {
        return *animationData.duration;
    }
    if (animationData.frameCount > 0) {
        return static_cast<float>(animationData.frameCount) / frameRate;
    }
    return 0.0f;
}

void shutdownAnimationResources() noexcept {
    placeholderTextureStorage().reset();
}
