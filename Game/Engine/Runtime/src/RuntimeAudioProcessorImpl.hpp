#pragma once

#include <Runtime/RuntimeAudioProcessor.hpp>
#include <SFML/Audio/SoundSource.hpp>

struct lua_State;

namespace ludork::runtime {

class AudioControlImpl {
public:
    explicit AudioControlImpl(AudioProcessorControl control);
    bool isCancelled() const;
    void beginTail();
    void finishTail();

private:
    AudioProcessorControl control_;
};

struct AudioProcessor::Impl {
public:
    explicit Impl(AudioProcessorOptions options);
    ~Impl();

    void process(const float* inputFrames, unsigned int& inputFrameCount,
                 float* outputFrames, unsigned int& outputFrameCount,
                 unsigned int frameChannelCount) noexcept;
    [[nodiscard]] std::optional<std::string> takeDeferredError() const;

private:
    lua_State* state_ = nullptr;
    sf::SoundSource::EffectProcessor processor_;
};

}  // namespace ludork::runtime
