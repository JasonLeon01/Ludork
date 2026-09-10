#pragma once

#include <Graphics/AnimationSoundEntry.hpp>

#include <string>
#include <vector>

namespace ludork::engine::animation_compression {

struct SegmentTransform {
    float x;
    float y;
    float rotation;
    float scaleX;
    float scaleY;
};

struct CompressedAnimationFrames {
    float duration = 0.0f;
    std::vector<std::string> frames;
    std::vector<AnimationSoundEntry> sounds;
};

}  // namespace ludork::engine::animation_compression
