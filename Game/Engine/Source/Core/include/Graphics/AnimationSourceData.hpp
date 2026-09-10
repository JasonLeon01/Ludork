#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <Graphics/AnimationTimeTag.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct AnimationSourceData {
    BIND_CLASS(copyable = true, table_init = true)
    struct AnimationTimeline {
        BIND_CLASS(copyable = true, table_init = true)
        struct AnimationSegment {
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

            BIND_PROPERTY()
            std::string type = "frame";

            BIND_PROPERTY()
            int asset = -1;

            BIND_PROPERTY()
            AnimationSourceData::AnimationTimeline::AnimationSegment::
                AnimationKeyFrame startFrame;

            BIND_PROPERTY()
            AnimationSourceData::AnimationTimeline::AnimationSegment::
                AnimationKeyFrame endFrame;

            BIND_PROPERTY()
            bool flipX = false;

            BIND_PROPERTY()
            std::optional<float> originalDuration;
        };

        BIND_PROPERTY()
        std::vector<AnimationSourceData::AnimationTimeline::AnimationSegment>
            timeSegments;
    };

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
    std::vector<AnimationSourceData::AnimationTimeline> timeLines;

    BIND_PROPERTY()
    std::vector<std::string> assets;
};
