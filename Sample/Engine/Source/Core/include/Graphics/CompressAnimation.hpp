#pragma once

#include <CoreMinimal.hpp>
#include <AnimSprite.hpp>
#include <Graphics/AnimationSourceData.hpp>

BIND_FUNCTION(returns = "duration,frames,sounds", allow_nil = "zlibModule")
std::tuple<float, std::vector<std::string>, std::vector<AnimationSoundEntry>>
C_CompressAnimation(
    const RuntimeValue& zlibModule, int frameCount, float frameStep,
    int frameRate,
    const std::vector<AnimationSourceData::AnimationTimeline>& timeLines,
    const std::vector<std::string>& assets, const std::string& imageFormat);

BIND_FUNCTION(name = "compressAnimation")
AnimSprite::AnimationData compressAnimation(
    std::optional<AnimationSourceData> source,
    const std::string& imageFormat = "png");
