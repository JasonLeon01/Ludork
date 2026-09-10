#pragma once

#include <Runtime/RuntimeAudioProcessor.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

class AudioEffectControl;

namespace ludork::global::audio {

class LuaAudioEffectProcessorImpl final {
public:
    LuaAudioEffectProcessorImpl(
        const std::string& name,
        const std::shared_ptr<AudioEffectControl>& control,
        std::uint32_t sampleRate, const std::string& packagePath);
    ~LuaAudioEffectProcessorImpl();
    LuaAudioEffectProcessorImpl(const LuaAudioEffectProcessorImpl&) = delete;
    LuaAudioEffectProcessorImpl& operator=(const LuaAudioEffectProcessorImpl&) =
        delete;

    void process(const float* inputFrames, unsigned int& inputFrameCount,
                 float* outputFrames, unsigned int& outputFrameCount,
                 unsigned int frameChannelCount) noexcept;
    std::optional<std::string> takeDeferredError() const;

private:
    ludork::runtime::AudioProcessor processor_;
};

}  // namespace ludork::global::audio
