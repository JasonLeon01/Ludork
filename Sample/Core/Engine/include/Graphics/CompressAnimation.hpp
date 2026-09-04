#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/System/Vector2.hpp>
#include <sol/forward.hpp>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

BIND_CLASS(copyable = true, table_init = true)
struct AnimationKeyFrame {
    BIND_PROPERTY()
    float time = 0.0f;

    BIND_PROPERTY()
    std::array<float, 2> position = {0.0f, 0.0f};

    BIND_PROPERTY()
    float rotation = 0.0f;

    BIND_PROPERTY()
    std::array<float, 2> scale = {1.0f, 1.0f};
};

BIND_CLASS(copyable = true, table_init = true)
struct AnimationSegment {
    BIND_PROPERTY()
    std::string type = "frame";

    BIND_PROPERTY()
    int asset = -1;

    BIND_PROPERTY()
    AnimationKeyFrame startFrame;

    BIND_PROPERTY()
    AnimationKeyFrame endFrame;

    BIND_PROPERTY()
    bool flipX = false;

    BIND_PROPERTY()
    std::optional<float> originalDuration;
};

BIND_CLASS(copyable = true, table_init = true)
struct AnimationTimeline {
    BIND_PROPERTY()
    std::vector<AnimationSegment> timeSegments;
};

BIND_CLASS(copyable = true, table_init = true)
struct AnimationTimeTag {
    BIND_PROPERTY()
    std::string tag;

    BIND_PROPERTY()
    float time = 0.0f;
};

BIND_CLASS(copyable = true, table_init = true)
struct AnimationSourceData {
    BIND_PROPERTY()
    std::string type;

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    int frameRate = 0;

    BIND_PROPERTY()
    int frameCount = 0;

    BIND_PROPERTY()
    std::optional<int> visualFrameCount;

    BIND_PROPERTY()
    std::optional<float> duration;

    BIND_PROPERTY()
    std::optional<float> visualDuration;

    BIND_PROPERTY()
    std::vector<AnimationTimeTag> timeTags;

    BIND_PROPERTY()
    std::vector<AnimationTimeline> timeLines;

    BIND_PROPERTY()
    std::vector<std::string> assets;
};

BIND_CLASS(copyable = true, table_init = true)
struct AnimationSoundEntry {
    BIND_PROPERTY()
    std::string asset;

    BIND_PROPERTY()
    int startFrame = 0;

    BIND_PROPERTY()
    int endFrame = 0;

    BIND_PROPERTY()
    std::optional<float> originalDuration;

    BIND_PROPERTY()
    std::optional<bool> stopAtEndFrame;
};

BIND_CLASS(copyable = true, table_init = true)
struct AnimationData {
    BIND_PROPERTY()
    std::string type = "compressedAnimation";

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    int frameRate = 30;

    BIND_PROPERTY()
    int frameCount = 0;

    BIND_PROPERTY()
    std::optional<int> visualFrameCount;

    BIND_PROPERTY()
    std::optional<float> duration;

    BIND_PROPERTY()
    std::optional<float> visualDuration;

    BIND_PROPERTY()
    std::vector<AnimationTimeTag> timeTags;

    BIND_PROPERTY()
    std::string frameEncoding = "base64+zlib";

    BIND_PROPERTY()
    std::vector<std::variant<std::string, std::shared_ptr<sf::Image>>> frames;

    BIND_PROPERTY()
    std::vector<AnimationSoundEntry> sounds;
};

BIND_FUNCTION(returns = "duration,frames,sounds")
std::tuple<float, std::vector<std::string>, std::vector<AnimationSoundEntry>>
C_CompressAnimation(const sol::object& zlibModule, int frameCount,
                    float frameStep, int frameRate,
                    const std::vector<AnimationTimeline>& timeLines,
                    const std::vector<std::string>& assets,
                    const std::string& imageFormat);

BIND_FUNCTION(name = "compressAnimation")
AnimationData compressAnimation(std::optional<AnimationSourceData> source,
                                const std::string& imageFormat = "png");
